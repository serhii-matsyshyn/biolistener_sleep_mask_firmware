import time
import threading
import logging
import sys
import select
import tty
import termios
import atexit
from mask_controller import MaskController

# Configure Logger
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('BrightnessCalibration')

class BrightnessCalibrator:
    def __init__(self):
        self.controller = MaskController()
        self.running = True
        self.current_relative_brightness = 0
        self.current_actual_brightness = 0
        self.hard_cap = 1.0
        self.step_percent = 5
        self.step_interval = 5.0
        
        # Strobe parameters
        self.frequency = 10.0
        self.fill = 0.1
        self.color = [0, 0, 0, 255] # White channel
        # self.mask = [True] * 8 # All LEDs
        self.mask = [False, False, True, True, True, True, False, False] # All LEDs

    def ramp_brightness(self):
        logger.info("Waiting for board connection...")
        while not self.controller.is_connected() and self.running:
            time.sleep(1)
        
        if not self.running:
            return

        logger.info(f"Board connected at {self.controller.board_ip}")
        logger.info("Starting brightness calibration cycle test...")

        while self.running:
            for relative_b in range(0, 105, self.step_percent):
                if not self.running:
                    break
                
                # Cap relative brightness at 100%
                if relative_b > 100:
                    relative_b = 100

                self.current_relative_brightness = relative_b
                # Calculate actual brightness: 0-100% relative -> 0-50% actual
                self.current_actual_brightness = (relative_b / 100.0) * self.hard_cap
                
                logger.info(f"Setting Brightness: Relative={self.current_relative_brightness}%, Actual={self.current_actual_brightness:.3f}")
                
                self.controller.update_params(
                    enabled=True,
                    frequency=self.frequency,
                    fill=self.fill,
                    brightness=self.current_actual_brightness,
                    color=self.color,
                    mask=self.mask
                )
                
                # Wait for step interval
                time.sleep(self.step_interval)
            
            logger.info("Cycle complete. Restarting ramp...")

    def start(self):
        # Save terminal settings
        old_settings = termios.tcgetattr(sys.stdin)
        def restore_settings():
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        atexit.register(restore_settings)
        
        # Set to cbreak mode to read keys without Enter
        tty.setcbreak(sys.stdin.fileno())

        ramp_thread = threading.Thread(target=self.ramp_brightness)
        ramp_thread.daemon = True
        ramp_thread.start()

        logger.info("Press ANY KEY to log a checkpoint. Press Ctrl+C to exit.")
        
        try:
            while self.running:
                if select.select([sys.stdin], [], [], 0.1)[0]:
                    sys.stdin.read(1) # Read one character
                    logger.info(f"USER CHECKPOINT: Relative={self.current_relative_brightness}%, Actual={self.current_actual_brightness:.3f}")
        except KeyboardInterrupt:
            logger.info("Exiting...")
        finally:
            self.running = False
            restore_settings()
            self.controller.stop()
            sys.exit(0)

if __name__ == "__main__":
    calibrator = BrightnessCalibrator()
    calibrator.start()
