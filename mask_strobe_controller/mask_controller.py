import socket
import json
import time
import threading
import logging

# Configure Logger
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('MaskController')


class MaskController:
    def __init__(self, controller_ip='0.0.0.0', port=20000):
        self.controller_ip = controller_ip
        self.port = port
        self.board_ip = None
        self.last_ping = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.controller_ip, self.port))
        self.sock.settimeout(1.0)
        self.running = True
        
        self.listener_thread = threading.Thread(target=self._listen)
        self.listener_thread.daemon = True
        self.listener_thread.start()

    def _listen(self):
        logger.info(f"Listening for board pings on port {self.port}...")
        while self.running:
            try:
                data, addr = self.sock.recvfrom(1024)
                try:
                    msg = json.loads(data.decode())
                    if msg.get('type') == 'ping':
                        if self.board_ip != addr[0]:
                            logger.info(f"Board discovered at {addr[0]}")
                        self.board_ip = addr[0]
                        self.last_ping = time.time()
                    elif msg.get('type') == 'params':
                        logger.info(f"Received params from {addr[0]}:")
                        if 'mask' in msg:
                            msg['mask_bin'] = bin(msg['mask'])
                        logger.info(json.dumps(msg, indent=4))
                except json.JSONDecodeError:
                    pass
            except socket.timeout:
                pass

    def send_command(self, command_dict):
        if not self.board_ip:
            logger.debug("Board IP not discovered yet.")
            return False
        
        try:
            msg = json.dumps(command_dict).encode()
            logger.debug(f"Sending: {msg}")
            self.sock.sendto(msg, (self.board_ip, self.port))
            return True
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return False

    def update_params(self, enabled=None, frequency=None, fill=None, brightness=None, color=None, mask=None):
        """
        Update strobe parameters.
        
        :param enabled: bool - Start (True) or Stop (False) the strobe
        :param frequency: float - Strobe frequency in Hz
        :param fill: float - Fill cycle ratio (0.0 to 1.0)
        :param brightness: float - Global brightness (0.0 to 1.0)
        :param color: list - [R, G, B] or [R, G, B, W] (0-255)
        :param mask: list of bool or int - Bitmask for individual LEDs
        """
        params = {"command": 0}  # CMD_CHANGE_PARAMS
        
        # Convert list bitmask to integer if provided
        if isinstance(mask, list):
            mask_int = 0
            for i, val in enumerate(mask):
                if val:
                    mask_int |= (1 << i)
            mask = mask_int
            logger.debug(f"Converted mask list to int: {mask}")

        if enabled is False:
            logger.info("STROBE DISABLED")

        # Filter out None values to only send requested changes
        updates = {
            "enabled": enabled,
            "frequency": frequency,
            "fill": fill,
            "brightness": brightness,
            "color": color,
            "mask": mask
        }
        params.update({k: v for k, v in updates.items() if v is not None})
        return self.send_command(params)

    def get_params(self):
        return self.send_command({"command": 1}) # CMD_GET_PARAMS

    def is_connected(self):
        return self.board_ip is not None and (time.time() - self.last_ping < 10)

    def stop(self):
        self.running = False
        self.sock.close()


if __name__ == "__main__":
    controller = MaskController()
    num_pixels = 8
    step = 0
    
    try:
        logger.info("Starting Mask Controller Demo...")
        while True:
            if not controller.is_connected():
                logger.info("Waiting for board...")
                time.sleep(2)
                continue

            logger.info(f"Board {controller.board_ip} connected. Sending step {step % 4}...")
            # mask = [True] * num_pixels
            # # only first 4 pixels enabled
            #
            # mask = [False, False, True, True, True, True, False, False]
            #
            # controller.update_params(
            #     enabled=True, frequency=10.0, fill=0.10, color=[0, 0, 0, 255], brightness=0.20,
            #     mask=mask
            # )
            # break
            
            # Demo logic
            case = step % 4
            if case == 0:
                logger.info("Setting frequency to 5Hz, Pure White (W channel)")
                mask = [(i % 2 == 0) for i in range(num_pixels)]
                controller.update_params(enabled=True, frequency=5.0, color=[0, 0, 0, 255], mask=mask)
            elif case == 1:
                logger.info("Setting frequency to 20Hz, Red")
                controller.update_params(frequency=20.0, color=[255, 0, 0, 0])
            elif case == 2:
                logger.info("Setting frequency to 2Hz, Green, 50% fill")
                controller.update_params(frequency=2.0, fill=0.5, color=[0, 255, 0, 0])
            elif case == 3:
                logger.info("Setting frequency to 10Hz, Blue, all LEDs")
                mask = [True] * num_pixels
                controller.update_params(frequency=10.0, fill=0.1, color=[0, 0, 255, 0], mask=mask)
            
            # Request params after update
            time.sleep(0.1)
            controller.get_params()
            
            step += 1
            time.sleep(5)
            
    except KeyboardInterrupt:
        controller.stop()
        logger.info("Stopped.")
