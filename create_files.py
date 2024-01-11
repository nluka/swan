from asyncio import sleep
import sys
import random
import string

def generate_random_name():
    return ''.join(random.choices(string.ascii_letters + string.digits, k=6))

if __name__ == "__main__":
    num_files = int(sys.argv[1]) if len(sys.argv) > 1 else 1000
    delay = float(sys.argv[2]) if len(sys.argv) > 2 else 0

    for _ in range(num_files):
        filename = generate_random_name()
        with open(filename, 'w'):
            pass
        print(f"Created file: {filename}")
        if delay > 0:
            sleep(delay)
