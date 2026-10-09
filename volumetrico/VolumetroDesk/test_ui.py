import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
import unittest
from pathlib import Path
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QFontDatabase
from app import Window, STYLE
from test_protocol import CONFIG


class FakePort:
    def __init__(self):self.output=[]
    def write(self,b):self.output.append(b);return len(b)
    def close(self):pass


class TestUI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app=QApplication.instance() or QApplication([])
        # Offscreen en Windows no enumera automáticamente las fuentes del sistema.
        for font in ('segoeui.ttf','segoeuib.ttf'):
            path=Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts'/font
            if path.exists():QFontDatabase.addApplicationFont(str(path))
        cls.app.setStyle('Fusion');cls.app.setStyleSheet(STYLE)

    def setUp(self):
        self.w=Window();self.w.timer.stop();self.w.port=FakePort()
        self.w.proto.start(['CONFIG VER'])
        for line in CONFIG:self.w.proto.feed(line)

    def tearDown(self):self.w.close()

    def test_slider_does_not_move_hardware(self):
        before=list(self.w.port.output);self.w.cards[0].angle.setValue(100)
        self.assertEqual(before,self.w.port.output)
        self.w.cards[0].move.click();self.assertEqual(self.w.port.output[-1],b'SERVO 1 IR 100\n')

    def test_error_still_allows_stop(self):
        self.w.proto.feed('ERR prueba');self.assertFalse(self.w.ready)
        self.w.stop.click();self.assertEqual(self.w.port.output[-1],b'STOP\n')

    def test_telemetry_and_config(self):
        self.assertEqual(self.w.references['X'].value(),17.5)
        self.w.proto.feed('SENSOR X ECO_US=725 CM=12.50 STATUS=ECO')
        self.assertEqual(self.w.sensor_labels['X'][0].text(),'12.50 cm')

    def test_design_roundtrip_and_preview_no_writes(self):
        self.w.add_step();d=self.w.design();self.assertEqual(d['steps'][0]['pause'],500)
        before=list(self.w.port.output);self.w.start_preview()
        self.assertEqual(before,self.w.port.output)

    def test_screenshot(self):
        self.w.show();self.app.processEvents()
        out=Path(__file__).parent/'qa';out.mkdir(exist_ok=True)
        self.assertTrue(self.w.grab().save(str(out/'servos.png')))
        from PySide6.QtWidgets import QTabWidget
        tabs=self.w.findChild(QTabWidget)
        tabs.setCurrentIndex(1);self.app.processEvents();self.w.grab().save(str(out/'mediciones.png'))
        tabs.setCurrentIndex(2);self.w.add_step();self.app.processEvents();self.w.grab().save(str(out/'diseno.png'))


if __name__=='__main__':unittest.main()
