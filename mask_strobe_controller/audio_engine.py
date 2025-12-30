import vlc
import os
import logging
import platform

logger = logging.getLogger('AudioEngine')


class AudioEngine:
    def __init__(self):
        # VLC flags for low-latency and high-priority audio
        vlc_flags = [
            '--no-video',
            '--quiet',
            '--file-caching=1000',
            '--clock-jitter=0',
        ]

        if platform.system() == 'Linux':
            vlc_flags.append('--aout=alsa')

        try:
            self.instance = vlc.Instance(*vlc_flags)
            self.player = self.instance.media_player_new()
        except Exception as e:
            logger.error(f"Failed to initialize VLC: {e}")
            raise

        self.current_track = None
        self.duration = 0
        self.is_playing = False

    def load_track(self, track_path):
        logger.info(f"Attempting to load track via VLC: {track_path}")
        if not os.path.exists(track_path):
            logger.error(f"Audio file not found at {track_path}")
            return False

        try:
            media = self.instance.media_new(track_path)
            self.player.set_media(media)

            # VLC needs to parse the media to get duration
            media.parse()
            # Duration is in milliseconds
            self.duration = media.get_duration() / 1000.0

            self.current_track = track_path
            self.is_playing = False
            logger.info(f"Successfully loaded track. Duration: {self.duration:.2f}s")
            return True
        except Exception as e:
            logger.error(f"VLC error loading {track_path}: {e}")
            return False

    def play(self, start_pos=0):
        if start_pos > 0:
            self.player.set_time(int(start_pos * 1000))
        self.player.play()
        self.is_playing = True

    def pause(self):
        self.player.pause()
        self.is_playing = False

    def stop(self):
        self.player.stop()
        self.is_playing = False

    def seek(self, pos):
        # pos is in seconds, VLC expects milliseconds
        self.player.set_time(int(pos * 1000))

    def get_current_pos(self):
        # VLC returns time in milliseconds
        t = self.player.get_time()
        if t < 0:
            return 0
        return t / 1000.0
