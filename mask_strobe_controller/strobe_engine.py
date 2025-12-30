import os
import yaml
import pandas as pd
import bisect

from mask_controller import MaskController
import logging

logger = logging.getLogger('StrobeEngine')


class StrobeEngine:
    def __init__(self, sessions_yaml_path, data_dir):
        self.sessions_yaml_path = sessions_yaml_path
        self.data_dir = data_dir
        self.sessions = self._load_sessions()
        self.current_strobe_data = None
        self.enabled = True
        self.brightness = 0.1
        self.color = [0, 0, 0, 255]  # R, G, B, W
        self.bitmask = 0xFF

        self.controller = MaskController()
        self.last_params = None

    def _load_sessions(self):
        with open(self.sessions_yaml_path, 'r') as f:
            return yaml.safe_load(f)

    def load_session_data(self, strobe_csv_rel_path):
        strobes_dir = self.sessions['configuration']['strobes_dir']
        csv_path = os.path.join(self.data_dir, strobes_dir, strobe_csv_rel_path)
        if not os.path.exists(csv_path):
            logger.error(f"Strobe CSV not found at {csv_path}")
            return False

        logger.info(f"Loading strobe data from {csv_path} into RAM...")
        df = pd.read_csv(csv_path)
        df = df.sort_values(by="Timestamp_NS").reset_index(drop=True)

        # Pre-calculate metrics
        df["Time_s"] = df["Timestamp_NS"] / 1_000_000_000.0

        cycles = []
        last_rise_time = None
        current_on_duration = 0

        for i in range(1, len(df)):
            prev_time = df.loc[i - 1, "Time_s"]
            curr_time = df.loc[i, "Time_s"]
            prev_state = df.loc[i - 1, "Torch_State"]
            curr_state = df.loc[i, "Torch_State"]

            if prev_state == 0 and curr_state == 1:
                if last_rise_time is not None:
                    period = curr_time - last_rise_time
                    if period > 0:
                        freq = 1.0 / period
                        duty = (current_on_duration / period)
                        if freq < 200:
                            cycles.append({
                                "Time_s": last_rise_time,
                                "Frequency_Hz": freq,
                                "Fill": duty
                            })
                last_rise_time = curr_time
                current_on_duration = 0
            elif prev_state == 1 and curr_state == 0:
                current_on_duration += (curr_time - prev_time)

        self.current_strobe_data = pd.DataFrame(cycles)
        return True

    def get_params_at_time(self, time_s):
        if self.current_strobe_data is None or self.current_strobe_data.empty:
            return None

        # Use binary search for faster lookup in sorted data
        times = self.current_strobe_data["Time_s"].values
        idx = bisect.bisect_right(times, time_s) - 1

        if idx < 0:
            return {"frequency": 0, "fill": 0}

        row = self.current_strobe_data.iloc[idx]
        return {
            "frequency": float(row["Frequency_Hz"]),
            "fill": float(row["Fill"])
        }

    def send_update(self, time_s, is_playing=True):
        params = self.get_params_at_time(time_s)
        if params is None:
            params = {"frequency": 0, "fill": 0}

        # Only send update if parameters changed significantly
        # Strobe is enabled only if both UI toggle is ON and audio is playing
        current_state = {
            "enabled": self.enabled and is_playing,
            "frequency": params["frequency"],
            "fill": params["fill"],
            "brightness": self.brightness,
            "color": self.color,
            "mask": self.bitmask
        }

        if self.last_params == current_state:
            return

        self.controller.update_params(**current_state)
        self.last_params = current_state

    def set_mask_ip(self, ip):
        self.controller.board_ip = ip
