import time
import threading
import os
import shutil
import sys
import miniprof
import miniprof.sampler

# Walltime-based sample event
def _sample_event():
    print("Profiling started.")
    while True:
        miniprof.sampler.check_threads()
        time.sleep(1/100)  # 10hz


def start():
    sample_thread = threading.Thread(target=_sample_event)
    sample_thread.start()


def _file_or_other(p):
    if os.path.isfile(p):
        return p
    return shutil.which(p)

def _bootstrap(dir):
    ppath = os.environ.get("PYTHONPATH", "")
    if ppath:
        os.environ["PYTHONPATH"] = f"{dir}:{ppath}"
    else:
        os.environ["PYTHONPATH"] = f"{dir}"


def main():
    target = _file_or_other(sys.argv[1])
    mod_root = os.path.dirname(miniprof.__file__)
    _bootstrap(os.path.join(mod_root, "bootstrap"))

    if not target:
        print("Error executing command")
    else:
        try:
            print(f"Starting {target}")
            os.execl(target, target, *sys.argv[2:])
            sys.exit(0)
        except:
            pass
    sys.exit(1)
