"""Protocolo secuencial: una orden pendiente; IR espera su terminación."""
from collections import deque
import math
import re
import time

SERVO = re.compile(r'SERVO ([12]) LIMITES (\d+) (\d+) REPOSO (\d+) EMPUJE (\d+) FLAGS (\d+) ACOPLADO ([01]) ANGULO_ORDENADO (\d+)')


def validate_design(data):
    if not isinstance(data, dict) or data.get('version') != 1:
        raise ValueError('Formato de diseño no compatible.')
    steps = data.get('steps')
    if not isinstance(steps, list) or not 1 <= len(steps) <= 100:
        raise ValueError('El diseño debe contener entre 1 y 100 pasos.')
    clean = []
    for row in steps:
        if not isinstance(row, dict):
            raise ValueError('Paso inválido.')
        vals = [row.get(k) for k in ('s1', 's2', 'pause')]
        if any(type(v) is not int for v in vals):
            raise ValueError('Los ángulos y las pausas deben ser enteros.')
        if not (0 <= vals[0] <= 180 and 0 <= vals[1] <= 180 and 0 <= vals[2] <= 10000):
            raise ValueError('Ángulos 0–180° y pausa 0–10000 ms.')
        clean.append(dict(zip(('s1', 's2', 'pause'), vals)))
    return {'version': 1, 'steps': clean}


def sequence_commands(design, config):
    steps = validate_design(design)['steps']
    if config['stop'] or config['state']:
        raise ValueError('El Arduino está detenido u ocupado.')
    for s in config['servos'].values():
        if not s['attached']:
            raise ValueError('Acopla los dos servos antes de reproducir.')
    commands = []
    for row in steps:
        for i in (1, 2):
            s = config['servos'][i]
            if not s['low'] <= row[f's{i}'] <= s['high']:
                raise ValueError(f'Servo {i}: un paso supera sus límites confirmados.')
            commands.append(f"SERVO {i} IR {row[f's{i}']}")
        commands.append(row['pause'] / 1000)
    return commands


class Protocol:
    def __init__(self, write, event, clock=time.monotonic):
        self.write, self.event, self.clock = write, event, clock
        self.queue = deque()
        self.pending = None
        self.deadline = 0
        self.config = None
        self.snapshot = None

    @property
    def busy(self):
        return self.pending is not None or bool(self.queue)

    def clear(self):
        self.queue.clear()
        self.pending = None
        self.config = None
        self.snapshot = None

    def start(self, commands):
        if self.busy:
            raise ValueError('Espera a que termine la operación actual.')
        self.queue.extend(commands)
        self._next()

    def stop(self):
        self.queue.clear()
        self.pending = None
        self.snapshot = None
        self.queue.extend(['STOP', 'CONFIG VER'])
        self._next()

    def _next(self):
        if not self.queue:
            self.pending = None
            self.event('done', '')
            return
        self.pending = self.queue.popleft()
        if isinstance(self.pending, (float, int)):
            self.deadline = self.clock() + self.pending
            self.event('progress', 'Pausa entre pasos')
            return
        self.deadline = self.clock() + 20
        if self.pending == 'CONFIG VER':
            self.snapshot = None
        self.write(self.pending)
        self.event('progress', self.pending)

    def tick(self):
        if self.pending is None or self.clock() < self.deadline:
            return
        if isinstance(self.pending, (float, int)):
            self._next()
        else:
            self.clear()
            self.event('timeout', 'Sin respuesta completa del Arduino. Secuencia cancelada.')

    def feed(self, line):
        if line.startswith('SENSOR '):
            self.event('sensor', line)
            return
        if line.startswith('RESULTADO '):
            self.event('result', line)
        if line.startswith('ERR '):
            # Una respuesta antigua no debe consumir la confirmación del STOP prioritario.
            if self.pending == 'STOP':
                self.event('log', line)
                return
            self.queue.clear()
            self.pending = None
            self.config = None
            self.event('error', line)
            return
        if self.pending == 'CONFIG VER':
            if line.startswith('CONFIG FW='):
                self.snapshot = {'firmware': line.split()[1][3:], 'refs': {}, 'servos': {}}
            elif self.snapshot is not None:
                match = SERVO.fullmatch(line)
                if match:
                    sid, lo, hi, rest, push, flags, active, angle = map(int, match.groups())
                    self.snapshot['servos'][sid] = dict(low=lo, high=hi, rest=rest, push=push, flags=flags, attached=bool(active), angle=angle)
                elif re.fullmatch(r'CAL [XYZ] \d+(?:\.\d+)?', line):
                    _, axis, value = line.split()
                    self.snapshot['refs'][axis] = float(value)
                elif re.fullmatch(r'ESTADO \d+ STOP [01]', line):
                    parts = line.split()
                    self.snapshot.update(state=int(parts[1]), stop=bool(int(parts[3])))
                    c = self.snapshot
                    if c['firmware'] != '1.1.0' or len(c['refs']) != 3 or len(c['servos']) != 2:
                        self.clear()
                        self.event('error', 'Carga Volumetrico_UNO 1.1.0 para utilizar esta app.')
                        return
                    if any(not 4 <= x <= 100 or not math.isfinite(x) for x in c['refs'].values()):
                        self.clear(); self.event('error', 'Referencias recibidas inválidas.'); return
                    for s in c['servos'].values():
                        if not (0 <= s['low'] < s['high'] <= 180 and 0 <= s['angle'] <= 180 and 0 <= s['flags'] <= 3):
                            self.clear(); self.event('error', 'Límites recibidos inválidos.'); return
                    self.config = c
                    self.event('config', c)
                    self._next()
            return
        if not isinstance(self.pending, str):
            return
        cmd = self.pending
        if ' IR ' in cmd:
            complete = line == 'OK movimiento terminado (orden)'
        elif cmd in ('LEER', 'MEDIR'):
            complete = line.startswith('RESULTADO ')
        else:
            expected = {
                'STOP': 'OK STOP pulsos desactivados',
                'REANUDAR': 'OK habilitado, servos siguen libres',
                'GUARDAR': 'OK EEPROM guardada', 'CARGAR': 'OK CARGAR',
                'VIVO ON': 'OK VIVO ON', 'VIVO OFF': 'OK VIVO OFF',
            }.get(cmd)
            if cmd.startswith('CAL '): expected = 'OK CAL RAM'
            if ' ACOPLAR ' in cmd: expected = 'OK ACOPLAR; posicion ordenada, no verificada'
            if cmd.endswith(' LIBERAR'): expected = 'OK LIBERAR'
            if ' LIMITES ' in cmd: expected = 'OK LIMITES; volver a fijar posiciones'
            if ' FIJAR ' in cmd: expected = 'OK posicion en RAM; falta GUARDAR'
            complete = expected is not None and line == expected
        if complete:
            self.event('ack', cmd)
            self._next()
