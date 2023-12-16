import subprocess
import time


def wait_sbatch(initial_wait=5, check_interval=2, timeout=180):
    # giving sbatch job to be issued
    time.sleep(initial_wait)

    # now waiting begins
    num_iters = timeout // check_interval
    for _ in range(num_iters):
        output = subprocess.check_output('squeue --me -h -t R'.split()).decode('utf-8')
        num_running = len(output.split('\n')) - 1
        if num_running > 0:
            time.sleep(check_interval)
        else:
            return True
    return False


def sbatch(cmd):
    print(f'Running : {cmd}')
    subprocess.run(cmd.split())
    if wait_sbatch():
        print(f'Finished!')
        return True
    else:
        print(f'Timeout!')
        return False
    


