import json
import math
from pathlib import Path
import re
import sys
import time

import serial
from serial.tools import list_ports
from PySide6.QtCore import Qt, QTimer, QPointF
from PySide6.QtGui import QColor, QPainter, QPen, QFont
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGridLayout, QLabel, QPushButton, QComboBox, QSlider, QSpinBox, QDoubleSpinBox,
    QGroupBox, QPlainTextEdit, QTabWidget, QTableWidget, QTableWidgetItem,
    QHeaderView, QFileDialog, QMessageBox, QScrollArea, QAbstractItemView)

from protocol import Protocol, validate_design, sequence_commands

STYLE = '''
QWidget { background:#0d1525; color:#e6edf7; font-family:'Segoe UI'; font-size:13px; }
QLabel { background:transparent; }
QLabel#brand { font-size:27px; font-weight:700; }
QLabel#muted { color:#9dafc8; }
QLabel#status { background:#172941; padding:10px; border-radius:7px; }
QGroupBox { border:1px solid #304057; border-radius:9px; margin-top:14px; padding:16px 10px 8px; font-weight:600; }
QGroupBox::title { subcontrol-origin:margin; left:12px; padding:0 5px; color:#8acbff; }
QPushButton { background:#23344e; border:1px solid #3a506e; border-radius:6px; padding:8px 12px; }
QPushButton:hover { background:#314b6f; }
QPushButton:disabled { color:#758094; background:#172132; border-color:#263449; }
QPushButton#primary { background:#176c87; border-color:#2b9eb7; }
QPushButton#stop { background:#aa3449; color:white; font-weight:700; font-size:17px; }
QComboBox,QSpinBox,QDoubleSpinBox { background:#17243a; border:1px solid #3a506e; border-radius:4px; padding:6px; }
QSlider::groove:horizontal { height:6px; background:#30415c; border-radius:3px; }
QSlider::handle:horizontal { width:17px; background:#65d1df; margin:-6px 0; border-radius:8px; }
QTabWidget::pane { border:1px solid #304057; border-radius:8px; }
QTabBar::tab { padding:11px 19px; background:#142238; }
QTabBar::tab:selected { background:#28415e; color:#7ce0e8; }
QPlainTextEdit,QTableWidget { background:#101d30; border:1px solid #304057; selection-background-color:#285b78; }
QHeaderView::section { background:#21314b; color:#cdd8e9; padding:7px; border:0; }
'''


def label(text, name=''):
    x = QLabel(text)
    if name: x.setObjectName(name)
    x.setWordWrap(True)
    return x


def button(text, fn, name=''):
    x = QPushButton(text); x.clicked.connect(lambda checked=False: fn())
    if name: x.setObjectName(name)
    return x


def spin(lo, hi, val, suffix='°'):
    x = QSpinBox(); x.setRange(lo, hi); x.setValue(val); x.setSuffix(suffix)
    return x


def add_tab(tabs, page, title):
    scroll = QScrollArea(); scroll.setWidgetResizable(True); scroll.setWidget(page)
    tabs.addTab(scroll, title)


class ArmView(QWidget):
    """Vista angular ilustrativa: no infiere posiciones físicas de ejes ni colisiones."""
    def __init__(self):
        super().__init__(); self.angles = [90, 90]; self.setMinimumHeight(180)

    def paintEvent(self, event):
        p = QPainter(self); p.setRenderHint(QPainter.Antialiasing)
        for i, (value, length, color) in enumerate(zip(self.angles, (12, 6.5), ('#68d8df', '#b5a1ff'))):
            cx, cy = self.width()*(0.25+0.5*i), 105
            radius = min(68, self.width()/5)
            p.setBrush(Qt.NoBrush);p.setPen(QPen(QColor('#30415c'), 1)); p.drawEllipse(QPointF(cx, cy), radius, radius)
            r = radius * length/12
            x, y = cx+r*math.cos(math.radians(value)), cy-r*math.sin(math.radians(value))
            p.setPen(QPen(QColor(color), 7, Qt.SolidLine, Qt.RoundCap)); p.drawLine(QPointF(cx, cy), QPointF(x, y))
            p.setBrush(QColor(color)); p.drawEllipse(QPointF(cx, cy), 5, 5)
            p.setPen(QColor('#e6edf7')); p.setFont(QFont('Segoe UI', 10))
            p.drawText(int(cx-83), 22, f'Servo {i+1} · {length:g} cm · {value}°')


class ServoCard(QGroupBox):
    def __init__(self, sid, owner):
        super().__init__(f'Servo {sid}  ·  {12 if sid == 1 else 6.5:g} cm  ·  D{sid+7}')
        self.sid, self.owner = sid, owner
        v = QVBoxLayout(self)
        self.feedback = label('Sin información del Arduino', 'muted'); v.addWidget(self.feedback)
        row = QHBoxLayout(); self.slider = QSlider(Qt.Horizontal); self.slider.setRange(0,180); self.slider.setValue(90)
        self.angle = spin(0,180,90); row.addWidget(self.slider,1); row.addWidget(self.angle); v.addLayout(row)
        self.slider.setAccessibleName(f'Ángulo objetivo servo {sid}')
        self.slider.valueChanged.connect(self.angle.setValue); self.angle.valueChanged.connect(self.slider.setValue)
        self.angle.valueChanged.connect(owner.draw_targets)
        row = QHBoxLayout()
        row.addWidget(button('−1°', lambda: self.angle.setValue(self.angle.value()-1)))
        row.addWidget(button('+1°', lambda: self.angle.setValue(self.angle.value()+1)))
        self.move = button('Mover', lambda: owner.action(f'SERVO {sid} IR {self.angle.value()}'), 'primary'); row.addWidget(self.move)
        self.attach = button('Acoplar', lambda: owner.action(f'SERVO {sid} ACOPLAR {self.angle.value()}')); row.addWidget(self.attach)
        self.release = button('Liberar', lambda: owner.action(f'SERVO {sid} LIBERAR')); row.addWidget(self.release); v.addLayout(row)
        row = QHBoxLayout()
        self.rest = button('Fijar reposo actual', lambda: owner.action(f'SERVO {sid} FIJAR REPOSO'))
        self.push = button('Fijar empuje actual', lambda: owner.action(f'SERVO {sid} FIJAR EMPUJE'))
        row.addWidget(self.rest); row.addWidget(self.push); v.addLayout(row)
        row = QHBoxLayout(); row.addWidget(label('Límites'))
        self.low = spin(0,179,20); self.high = spin(1,180,160)
        row.addWidget(self.low); row.addWidget(self.high)
        self.limits = button('Aplicar límites', self.set_limits); row.addWidget(self.limits); v.addLayout(row)
        self.saved = label('Reposo: —   ·   Empuje: —', 'muted'); v.addWidget(self.saved)

    def set_limits(self):
        if self.low.value() >= self.high.value():
            self.owner.notice('El límite mínimo debe ser menor que el máximo.'); return
        self.owner.action(f'SERVO {self.sid} LIMITES {self.low.value()} {self.high.value()}')

    def sync(self, s):
        self.feedback.setText(f"{'Acoplado' if s['attached'] else 'Libre'} · ángulo ordenado {s['angle']}° (no medido)")
        self.low.setValue(s['low']); self.high.setValue(s['high'])
        self.slider.setRange(s['low'],s['high']); self.angle.setRange(s['low'],s['high'])
        rest = str(s['rest'])+'°' if s['flags'] & 1 else 'sin definir'
        push = str(s['push'])+'°' if s['flags'] & 2 else 'sin definir'
        self.saved.setText(f'Reposo: {rest}  ·  Empuje: {push}')


class Window(QMainWindow):
    def __init__(self):
        super().__init__(); self.setWindowTitle('VolumetroDesk · CELE'); self.resize(1130,760)
        self.port = None; self.buffer = b''; self.boot_until = None; self.ready = False
        self.preview = []; self.preview_deadline = 0; self.live_seen = {}; self.cards = []
        self.proto = Protocol(self.write_line, self.on_event)
        root = QWidget(); self.setCentralWidget(root); v = QVBoxLayout(root)
        top = QHBoxLayout(); title = QVBoxLayout(); title.addWidget(label('VolumetroDesk', 'brand'))
        title.addWidget(label('CELE  /  Medición y diseño de movimiento', 'muted')); top.addLayout(title,1)
        self.stop = button('■  STOP', self.emergency, 'stop'); self.stop.setMinimumWidth(140); top.addWidget(self.stop); v.addLayout(top)
        conn = QHBoxLayout(); self.ports = QComboBox(); self.ports.setMinimumWidth(200); conn.addWidget(self.ports)
        conn.addWidget(button('Buscar puertos', self.refresh_ports)); self.connect_btn = button('Conectar USB', self.connect_port, 'primary'); conn.addWidget(self.connect_btn)
        self.refresh_btn = button('Consultar Arduino', self.refresh); conn.addWidget(self.refresh_btn)
        self.resume = button('Reanudar', lambda: self.action('REANUDAR')); conn.addWidget(self.resume); conn.addStretch(); v.addLayout(conn)
        self.status = label('Sin conexión · puedes diseñar y previsualizar sin hardware.', 'status'); v.addWidget(self.status)
        tabs = QTabWidget(); v.addWidget(tabs,1)
        control = QWidget(); layout = QVBoxLayout(control)
        row = QHBoxLayout()
        for i in (1,2):
            card = ServoCard(i,self); self.cards.append(card); row.addWidget(card)
        layout.addLayout(row)
        self.view = ArmView(); layout.addWidget(self.view)
        layout.addWidget(label('Vista angular de los objetivos, no simulación del montaje. Cambiar el deslizador no mueve el servo: pulsa Mover. El primer Acoplar puede saltar al ángulo indicado.', 'muted'))
        store = QHBoxLayout()
        self.save_eeprom = button('Guardar calibración en EEPROM', lambda: self.action('GUARDAR'), 'primary'); store.addWidget(self.save_eeprom)
        self.load_eeprom = button('Recuperar EEPROM', lambda: self.action('CARGAR')); store.addWidget(self.load_eeprom)
        self.eeprom_status = label('EEPROM: sin cambios enviados en esta sesión', 'muted'); store.addWidget(self.eeprom_status,1); layout.addLayout(store)
        layout.addStretch(); add_tab(tabs,control,'01  ·  Servos')
        self.build_sensors(tabs); self.build_design(tabs)
        self.log = QPlainTextEdit(); self.log.setReadOnly(True); self.log.setMaximumBlockCount(1000); self.log.setMaximumHeight(85)
        v.addWidget(self.log)
        foot = QHBoxLayout(); foot.addWidget(label('USB · 115200 baudios · firmware 1.1.0 · 2 servos / 3 sensores', 'muted'),1)
        foot.addWidget(button('Exportar registro', self.export_log)); v.addLayout(foot)
        self.timer = QTimer(self); self.timer.timeout.connect(self.poll); self.timer.start(25)
        self.refresh_ports(); self.update_controls()

    def build_sensors(self, tabs):
        page=QWidget(); v=QVBoxLayout(page); grid=QGridLayout(); self.sensor_labels={}; self.references={}
        for col,(axis,name,ref) in enumerate((('X','Altura',17.5),('Y','Largo',26),('Z','Ancho',16))):
            group=QGroupBox(f'{axis} · {name}'); box=QVBoxLayout(group)
            reading=label('— cm'); reading.setStyleSheet('font-size:28px; font-weight:600;'); box.addWidget(reading)
            detail=label('Sin lectura recibida','muted'); box.addWidget(detail)
            val=QDoubleSpinBox(); val.setRange(4,100); val.setDecimals(2); val.setValue(ref); val.setSuffix(' cm')
            box.addWidget(label('Distancia al plano de referencia')); box.addWidget(val)
            self.sensor_labels[axis]=(reading,detail); self.references[axis]=val; grid.addWidget(group,0,col)
        v.addLayout(grid); row=QHBoxLayout()
        self.live_on=button('Ver distancias en vivo',lambda:self.action('VIVO ON'),'primary')
        self.live_off=button('Detener lectura en vivo',lambda:self.action('VIVO OFF'))
        self.apply_refs=button('Aplicar referencias (RAM)', self.calibrate)
        row.addWidget(self.live_on);row.addWidget(self.live_off);row.addWidget(self.apply_refs);v.addLayout(row)
        v.addWidget(label('Las distancias en vivo no se filtran. Se muestran también ecos fuera del espacio calibrado; durante el movimiento se pausa la lectura. Para conservar las referencias, usa Guardar calibración en EEPROM en la pestaña Servos.','muted'))
        row=QHBoxLayout();self.read=button('Medir sin mover',lambda:self.action('LEER'),'primary')
        self.cycle=button('Alinear y medir',lambda:self.action('MEDIR'));row.addWidget(self.read);row.addWidget(self.cycle);v.addLayout(row)
        self.result=label('Volumen: sin medición validada');self.result.setStyleSheet('font-size:24px; padding:15px;');v.addWidget(self.result)
        v.addWidget(label('Medir sin mover requiere ambos servos libres. Alinear y medir requiere ambos acoplados, reposo y empuje definidos. Al finalizar el ciclo se liberan.','muted'));v.addStretch();add_tab(tabs,page,'02  ·  Mediciones')

    def build_design(self,tabs):
        page=QWidget();v=QVBoxLayout(page)
        v.addWidget(label('Diseña una secuencia de posiciones','brand'))
        v.addWidget(label('Cada paso mueve primero el servo 1 y después el 2; espera el fin de cada orden y luego la pausa. Los diseños se guardan en la PC.','muted'))
        self.table=QTableWidget(0,3);self.table.setHorizontalHeaderLabels(['Servo 1 · grados','Servo 2 · grados','Pausa · ms'])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch);self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection);v.addWidget(self.table,1)
        row=QHBoxLayout()
        for name,fn in [('Añadir objetivos',self.add_step),('Quitar paso',self.remove_step),('↑',lambda:self.shift_step(-1)),('↓',lambda:self.shift_step(1)),('Guardar diseño',self.save_design),('Abrir diseño',self.load_design)]:row.addWidget(button(name,fn))
        v.addLayout(row);row=QHBoxLayout()
        row.addWidget(button('Previsualizar sin mover',self.start_preview));row.addWidget(button('Cancelar vista',self.cancel_preview))
        self.play=button('Ejecutar en Arduino',self.play_design,'primary');row.addWidget(self.play);v.addLayout(row)
        self.design_view=ArmView();v.addWidget(self.design_view)
        self.design_status=label('Previsualización angular; no comprueba colisiones ni contacto.','muted');v.addWidget(self.design_status)
        add_tab(tabs,page,'03  ·  Diseñar movimiento')

    def draw_targets(self):
        if hasattr(self,'view'):
            self.view.angles=[c.angle.value() for c in self.cards];self.view.update()

    def notice(self,text):
        self.status.setText(text);self.log.appendPlainText('! '+text)

    def refresh_ports(self):
        current=self.ports.currentData();self.ports.clear()
        for p in list_ports.comports():self.ports.addItem(f'{p.device} · {p.description}',p.device)
        index=self.ports.findData(current)
        if index>=0:self.ports.setCurrentIndex(index)

    def connect_port(self):
        if self.port:self.disconnect();return
        if not self.ports.currentData():self.notice('No hay puerto USB disponible.');return
        try:
            self.port=serial.Serial(self.ports.currentData(),115200,timeout=0,write_timeout=0.2)
            self.proto.clear();self.ready=False;self.buffer=b'';self.boot_until=time.monotonic()+2.5
            self.notice('Conectado al puerto; esperando reinicio e identificación del Arduino…')
            self.connect_btn.setText('Desconectar');self.update_controls()
        except (OSError,serial.SerialException) as exc:self.notice(f'No se pudo abrir el puerto: {exc}')

    def disconnect(self):
        if self.port:
            try:
                if not self.boot_until:self.port.write(b'STOP\n')
            except (OSError,serial.SerialException):pass
            self.port.close()
        self.port=None;self.boot_until=None;self.ready=False;self.proto.clear();self.buffer=b'';self.live_seen.clear()
        for reading,detail in self.sensor_labels.values():reading.setText('— cm');detail.setText('Desconectado')
        self.result.setText('Volumen: sin lectura actual');self.connect_btn.setText('Conectar USB')
        self.notice('Desconectado. Si había movimiento, STOP enviado sin confirmación; verifica el equipo.');self.update_controls()

    def write_line(self,line):
        if not self.port:raise ValueError('No hay conexión.')
        data=(line+'\n').encode('ascii')
        if self.port.write(data)!=len(data):raise serial.SerialTimeoutException('Escritura incompleta')
        self.log.appendPlainText('→ '+line)

    def refresh(self):
        if self.port and not self.proto.busy and not self.boot_until:
            try:self.proto.start(['CONFIG VER'])
            except Exception as exc:self.notice(str(exc))
        self.update_controls()

    def action(self,*commands):
        if not self.ready or self.proto.busy:self.notice('Consulta el Arduino y espera que esté disponible.');return
        if any(c in ('LEER','MEDIR','VIVO ON') or ' IR ' in c or ' ACOPLAR ' in c or c.startswith('CAL ') for c in commands):
            self.result.setText('Volumen: pendiente de nueva medición')
        try:self.proto.start([*commands,'CONFIG VER'])
        except (ValueError,OSError,serial.SerialException) as exc:self.notice(str(exc))
        self.update_controls()

    def emergency(self):
        self.cancel_preview()
        if self.port and not self.boot_until:
            try:self.proto.stop();self.notice('STOP enviado; esperando confirmación del Arduino.')
            except (OSError,serial.SerialException) as exc:self.notice(str(exc));self.disconnect()
        self.update_controls()

    def calibrate(self):
        self.action(*(f'CAL {a} {v.value():.2f}' for a,v in self.references.items()))

    def on_event(self,kind,value):
        if kind=='config':
            self.ready=True
            for sid,s in value['servos'].items():self.cards[sid-1].sync(s)
            for a,r in value['refs'].items():self.references[a].setValue(r)
        elif kind=='ack':
            if value=='GUARDAR':self.eeprom_status.setText('EEPROM: guardado confirmado por Arduino')
            elif value.startswith('CAL ') or ' FIJAR ' in value or ' LIMITES ' in value:self.eeprom_status.setText('Cambios en RAM: falta guardar EEPROM')
            elif value=='CARGAR':self.eeprom_status.setText('Configuración recuperada de EEPROM')
            elif value in ('VIVO OFF','STOP'):
                self.live_seen.clear()
                for reading,detail in self.sensor_labels.values():detail.setText('Lectura detenida · dato anterior')
        elif kind=='progress':self.status.setText('En curso · '+str(value))
        elif kind=='done':
            if self.ready and self.proto.config:
                self.status.setText('STOP activo · pulsa Reanudar para habilitar.' if self.proto.config['stop'] else 'Arduino confirmado · listo. Ángulos ordenados; posición física no medida.')
        elif kind in ('error','timeout'):
            self.notice(str(value));self.result.setText('Sin resultado actual · revisar registro')
            if kind=='timeout':
                self.disconnect();self.notice(str(value)+' Conexión cerrada; comprueba el equipo.')
            else:
                self.ready=False
                # Mantener STOP disponible tras errores del protocolo; consultar restablece estado.
        elif kind=='sensor':
            match=re.fullmatch(r'SENSOR ([XYZ]) ECO_US=(\d+) CM=(NA|\d+(?:\.\d+)?) STATUS=(\w+)',value)
            if match:
                a,us,cm,status=match.groups();reading,detail=self.sensor_labels[a]
                reading.setText('Sin eco' if cm=='NA' else f'{cm} cm');detail.setText(f'{status} · {us} µs');self.live_seen[a]=time.monotonic()
        elif kind=='result':self.result.setText(value.replace('RESULTADO ','').replace(' V=','\nVolumen = '))
        self.update_controls()

    def update_controls(self):
        c=self.proto.config;available=bool(self.ready and c and not self.proto.busy and not self.boot_until)
        idle=bool(available and c['state']==0); enabled=bool(idle and not c['stop'])
        self.stop.setEnabled(bool(self.port and not self.boot_until))
        self.refresh_btn.setEnabled(bool(self.port and not self.proto.busy and not self.boot_until))
        self.resume.setEnabled(bool(idle and c['stop']))
        self.ports.setEnabled(not bool(self.port))
        for card in self.cards:
            s=c['servos'][card.sid] if c else None
            attached=bool(s and s['attached'])
            card.move.setEnabled(enabled and attached);card.attach.setEnabled(enabled and not attached)
            card.release.setEnabled(idle and attached);card.rest.setEnabled(enabled and attached);card.push.setEnabled(enabled and attached)
            card.limits.setEnabled(idle and not attached)
        for b in (self.save_eeprom,self.load_eeprom,self.apply_refs):b.setEnabled(idle)
        self.live_on.setEnabled(enabled);self.live_off.setEnabled(enabled)
        self.read.setEnabled(bool(enabled and all(not s['attached'] for s in c['servos'].values())))
        ready_cycle=bool(enabled and all(s['attached'] and s['flags']==3 and s['rest']!=s['push'] for s in c['servos'].values()))
        self.cycle.setEnabled(ready_cycle)
        self.play.setEnabled(bool(enabled and all(s['attached'] for s in c['servos'].values())))

    def poll(self):
        try:
            if self.port:
                if self.boot_until:
                    if time.monotonic()>=self.boot_until:
                        self.port.reset_input_buffer();self.buffer=b'';self.boot_until=None;self.proto.start(['CONFIG VER'])
                else:
                    self.buffer+=self.port.read(min(self.port.in_waiting,4096))
                    if len(self.buffer)>16384:raise ValueError('Entrada serie excesiva o sin fin de línea.')
                    while b'\n' in self.buffer:
                        raw,self.buffer=self.buffer.split(b'\n',1);line=raw.decode('ascii',errors='replace').strip()
                        if line:
                            self.log.appendPlainText('← '+line);self.proto.feed(line)
            self.proto.tick()
        except (OSError,serial.SerialException,ValueError) as exc:
            self.disconnect();self.notice(f'Conexión interrumpida: {exc}')
        now=time.monotonic()
        for a,t in list(self.live_seen.items()):
            if now-t>2:self.sensor_labels[a][1].setText('Dato anterior · sin actualización >2 s')
        if self.preview and now>=self.preview_deadline:
            step=self.preview.pop(0);self.design_view.angles=[step['s1'],step['s2']];self.design_view.update()
            self.design_status.setText(f"Vista sin hardware · S1 {step['s1']}° / S2 {step['s2']}° · pausa {step['pause']} ms")
            self.preview_deadline=now+max(0.5,step['pause']/1000)
            if not self.preview:self.design_status.setText('Última posición de la vista · no se enviaron movimientos.')

    def design(self):
        rows=[]
        try:
            for r in range(self.table.rowCount()):rows.append(dict(zip(('s1','s2','pause'),[int(self.table.item(r,c).text()) for c in range(3)])))
        except (ValueError,AttributeError):raise ValueError('Completa los pasos con números enteros.')
        return validate_design({'version':1,'steps':rows})

    def add_row(self,row):
        r=self.table.rowCount();self.table.insertRow(r)
        for c,k in enumerate(('s1','s2','pause')):self.table.setItem(r,c,QTableWidgetItem(str(row[k])))
        self.table.selectRow(r)

    def add_step(self):
        if self.table.rowCount()>=100:self.notice('Máximo 100 pasos.');return
        self.add_row({'s1':self.cards[0].angle.value(),'s2':self.cards[1].angle.value(),'pause':500})

    def remove_step(self):
        r=self.table.currentRow()
        if r>=0:self.table.removeRow(r)

    def shift_step(self,delta):
        r=self.table.currentRow();dest=r+delta
        if r<0 or not 0<=dest<self.table.rowCount():return
        for c in range(3):
            a=self.table.takeItem(r,c);b=self.table.takeItem(dest,c);self.table.setItem(r,c,b);self.table.setItem(dest,c,a)
        self.table.selectRow(dest)

    def save_design(self):
        try:data=self.design()
        except ValueError as exc:self.notice(str(exc));return
        path,_=QFileDialog.getSaveFileName(self,'Guardar diseño en PC','movimiento.json','Diseño JSON (*.json)')
        if path:
            try:Path(path).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');self.notice('Diseño guardado en PC; EEPROM sin cambios.')
            except OSError as exc:self.notice(str(exc))

    def load_design(self):
        path,_=QFileDialog.getOpenFileName(self,'Abrir diseño','','Diseño JSON (*.json)')
        if path:
            try:
                if Path(path).stat().st_size>100000:raise ValueError('Archivo demasiado grande.')
                data=validate_design(json.loads(Path(path).read_text(encoding='utf-8')))
                self.table.setRowCount(0)
                for row in data['steps']:self.add_row(row)
                self.notice('Diseño abierto; no se enviaron movimientos.')
            except (ValueError,OSError) as exc:self.notice(str(exc))

    def start_preview(self):
        try:self.preview=self.design()['steps'].copy();self.preview_deadline=0
        except ValueError as exc:self.notice(str(exc))

    def cancel_preview(self):
        self.preview=[]
        if hasattr(self,'design_status'):self.design_status.setText('Vista detenida; no afecta al hardware.')

    def play_design(self):
        if not self.ready or not self.proto.config or self.proto.busy:return
        try:
            commands=sequence_commands(self.design(),self.proto.config)
            self.cancel_preview();self.result.setText('Volumen: pendiente de nueva medición')
            self.proto.start(['VIVO OFF',*commands,'CONFIG VER'])
        except ValueError as exc:self.notice(str(exc))
        self.update_controls()

    def export_log(self):
        path,_=QFileDialog.getSaveFileName(self,'Exportar registro','registro_volumetrico.txt','Texto (*.txt)')
        if path:
            try:Path(path).write_text(self.log.toPlainText(),encoding='utf-8')
            except OSError as exc:self.notice(str(exc))

    def closeEvent(self,event):
        self.disconnect();event.accept()


if __name__=='__main__':
    app=QApplication(sys.argv);app.setStyle('Fusion');app.setStyleSheet(STYLE)
    window=Window()
    if len(sys.argv)==3 and sys.argv[1]=='--self-test':
        window.timer.stop()
        Path(sys.argv[2]).write_text(json.dumps({'servos':len(window.cards),'sensores':list(window.references),'conexion_automatica':window.port is not None}),encoding='utf-8')
        QTimer.singleShot(10,app.quit)
    else:window.show()
    sys.exit(app.exec())
