import time
import threading
import os
import shutil
import sys
from miniprof import sampler

# Walltime-based sample event
def sample_event():
    while True:
        sampler.check_threads()
        time.sleep(1/10)  # 10hz


def file_or_other(p):
    if os.path.isfile(p):
        return p
    return shutil.which(p)


def main():
    script_name = sys.argv[0]
    target = file_or_other(sys.argv[0])
    if not target:
        print("Error executing command")
    else:
        print("Profiling started")
        sample_thread = threading.Thread(target=sample_event)
        sample_thread.start()

        try:
            os.execl(executable, executable, *sys.argv[1:])
            sys.exit(0)
        except:
            pass

    sys.exit(1)
