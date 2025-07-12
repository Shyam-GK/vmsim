# Virtual Memory Paging Simulator

This project simulates **virtual memory paging** with multiple page replacement algorithms and provides **visual and interactive** insights into how memory accesses are managed at the OS level.

## 🧠 Features

- Simulates various page replacement algorithms:
  - FIFO (First-In-First-Out)
  - LRU (Least Recently Used)
  - MIN (Optimal)
  - CLOCK
  - Second Chance
- Captures **live memory access traces** from the Linux `/proc` filesystem.
- Provides:
  - **Graphical visualization** using Gnuplot.
  - **Interactive animation** using SDL2.
- Tracks detailed statistics:
  - Hits
  - Misses
  - Page faults
  - Hit and miss ratios

## 📦 Dependencies

Install the following packages (on Debian/Ubuntu-based systems):

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev gnuplot
🛠️ Compilation
Compile the program using:

bash
Copy
Edit
gcc -o vmsim main.c -lSDL2 -lSDL2_ttf
Replace main.c with your actual source filename if different.

▶️ Usage
bash
Copy
Edit
./vmsim <algorithm> <physical_address_bits>
Parameters:

<algorithm>:

0 – FIFO

1 – LRU

2 – MIN

3 – Second Chance

4 – CLOCK

<physical_address_bits>:

20 – 1 MB physical memory

24 – 16 MB physical memory

Example
bash
Copy
Edit
./vmsim 1 24
This runs the simulator using the LRU algorithm with 24-bit physical addressing.

📊 Output
Console output: Statistics on memory accesses

Gnuplot graph: Static visualization of memory trace over time

SDL2 window: Real-time animation of page table activity

Controls in SDL2 Visualization
Space: Step or start autoplay

P: Pause or resume autoplay

+ / -: Adjust animation speed

ESC: Exit visualization

📝 Notes
By default, memory trace is collected from /proc/640/maps or /proc/641/maps. Modify the PID in the source to analyze a different process.

You may need root permissions to access memory maps of certain processes.

🧹 Cleanup
To remove the compiled output and generated files:

bash
Copy
Edit
rm vmsim plot.txt
📄 License
This project is intended for academic and educational use in studying Operating System concepts, especially paging and page replacement algorithms.
