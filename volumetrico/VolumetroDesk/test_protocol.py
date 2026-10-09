import unittest
from protocol import Protocol, validate_design, sequence_commands

CONFIG = [
    'CONFIG FW=1.1.0 X=ALTURA Y=LARGO Z=ANCHO',
    'CAL X 17.50','CAL Y 26.00','CAL Z 16.00',
    'SERVO 1 LIMITES 20 160 REPOSO 90 EMPUJE 120 FLAGS 3 ACOPLADO 1 ANGULO_ORDENADO 90',
    'SERVO 2 LIMITES 20 160 REPOSO 90 EMPUJE 60 FLAGS 3 ACOPLADO 1 ANGULO_ORDENADO 90',
    'ESTADO 0 STOP 0',
]


class TestProtocol(unittest.TestCase):
    def setUp(self):
        self.sent=[];self.events=[];self.now=0
        self.p=Protocol(self.sent.append,lambda *x:self.events.append(x),lambda:self.now)

    def configure(self):
        self.p.start(['CONFIG VER'])
        for line in CONFIG:self.p.feed(line)

    def test_identification_and_axis_map(self):
        self.configure()
        self.assertEqual(self.p.config['refs'],{'X':17.5,'Y':26.,'Z':16.})
        self.assertFalse(self.p.busy)

    def test_old_firmware_not_accepted(self):
        self.p.start(['CONFIG VER'])
        for line in CONFIG:self.p.feed(line.replace('1.1.0','1.0.1'))
        self.assertIsNone(self.p.config)
        self.assertEqual(self.events[-1][0],'error')

    def test_ir_waits_completion_not_ack(self):
        self.p.start(['SERVO 1 IR 100','SERVO 2 IR 80'])
        self.p.feed('OK IR');self.assertEqual(len(self.sent),1)
        self.p.feed('SENSOR X ECO_US=725 CM=12.50 STATUS=ECO')
        self.assertEqual(len(self.sent),1)
        self.p.feed('OK movimiento terminado (orden)')
        self.assertEqual(self.sent[-1],'SERVO 2 IR 80')

    def test_stop_cancels_remaining_moves_and_ignores_stale_ack(self):
        self.p.start(['SERVO 1 IR 100','SERVO 2 IR 80'])
        self.p.stop();self.p.feed('OK movimiento terminado (orden)')
        self.p.feed('ERR ocupado; STOP para interrumpir')
        self.assertEqual(self.p.pending,'STOP')
        self.p.feed('OK STOP pulsos desactivados')
        self.assertEqual(self.sent,['SERVO 1 IR 100','STOP','CONFIG VER'])

    def test_error_aborts_queue(self):
        self.p.start(['SERVO 1 IR 100','SERVO 2 IR 80'])
        self.p.feed('ERR servo bloqueado')
        self.assertFalse(self.p.busy);self.assertEqual(len(self.sent),1)

    def test_timeout_aborts_queue(self):
        self.p.start(['SERVO 1 IR 100','GUARDAR']);self.now=21;self.p.tick()
        self.assertFalse(self.p.busy);self.assertEqual(self.events[-1][0],'timeout')

    def test_pause_does_not_block_stop(self):
        self.p.start([10.,'SERVO 1 IR 100']);self.p.stop()
        self.assertEqual(self.sent,['STOP'])

    def test_save_requires_real_confirmation(self):
        self.p.start(['GUARDAR']);self.p.feed('OK IR')
        self.assertTrue(self.p.busy)
        self.p.feed('OK EEPROM guardada');self.assertFalse(self.p.busy)

    def test_read_waits_result(self):
        self.p.start(['LEER','CONFIG VER']);self.p.feed('OK LEER')
        self.assertEqual(self.sent,['LEER'])
        self.p.feed('RESULTADO L=3 A=3 H=5 cm V=45 cm3')
        self.assertEqual(self.sent[-1],'CONFIG VER')

    def test_reject_incomplete_config(self):
        self.p.start(['CONFIG VER'])
        for line in CONFIG:
            if not line.startswith('SERVO 2'):self.p.feed(line)
        self.assertIsNone(self.p.config)

    def test_sequence_checks_limits_and_attachment(self):
        self.configure();d={'version':1,'steps':[{'s1':100,'s2':80,'pause':500}]}
        self.assertEqual(sequence_commands(d,self.p.config),['SERVO 1 IR 100','SERVO 2 IR 80',0.5])
        d['steps'][0]['s1']=170
        with self.assertRaises(ValueError):sequence_commands(d,self.p.config)
        d['steps'][0]['s1']=100;self.p.config['servos'][2]['attached']=False
        with self.assertRaises(ValueError):sequence_commands(d,self.p.config)

    def test_design_rejects_malformed_or_injected_commands(self):
        for data in [None,{}, {'version':1,'steps':[]}, {'version':1,'steps':[{'s1':'90\nMEDIR','s2':90,'pause':0}]}, {'version':1,'steps':[{'s1':True,'s2':90,'pause':0}]}]:
            with self.assertRaises(ValueError):validate_design(data)


if __name__=='__main__':unittest.main()
