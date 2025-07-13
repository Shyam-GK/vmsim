🧠 Virtual Memory Paging Simulator
==================================

This project simulates **virtual memory paging** with multiple **page replacement algorithms** and provides **visual and interactive insights** into how memory accesses are managed at the operating system level.

🚀 Features
-----------

### 🔁 Page Replacement Algorithms

*   FIFO (First-In-First-Out)
    
*   LRU (Least Recently Used)
    
*   MIN (Optimal)
    
*   CLOCK
    
*   Second Chance
    

### 📥 Memory Trace Collection

*   Captures live memory access traces from the Linux /proc filesystem
    
*   Default PID: 641
    

### 📊 Visualization

*   **Gnuplot**: Static line plot of memory accesses (time vs. page number)
    
*   **SDL2 GUI**:
    
    *   Bar graph showing hits, misses, and page fault ratios
        
    *   Real-time animation of memory frames:
        
        *   ✅ Green: Page hit
            
        *   ❌ Red: Page miss
            

### 📈 Statistics

*   Tracks:
    
    *   Hits
        
    *   Misses
        
    *   Page faults
        
    *   Hit ratio
        
    *   Miss ratio
        
*   Outputs detailed metrics to the console
    

### 🕹️ Interactive Controls

*   Toggle between graph and animation views
    
*   Step through memory accesses or autoplay
    
*   Adjust animation speed interactively
    

### 🧠 Configurable Memory

*   Supports:
    
    *   20-bit → 1 MB physical memory
        
    *   24-bit → 16 MB physical memory
        
*   Page size: 4 KB
    

📦 Dependencies
---------------

Install required packages (for Debian/Ubuntu-based systems):

`   sudo apt update  sudo apt install build-essential libsdl2-dev libsdl2-ttf-dev gnuplot   `

🛠️ Compilation
---------------

Compile the program using:

`   gcc -o vmsim main.c -lSDL2 -lSDL2_ttf   `

Replace main.c with the actual source filename if different.

▶️ Usage
--------

Run the simulator with:

`./vmsim`  

### Parameters

*   :
    
    *   0: FIFO
        
    *   1: LRU
        
    *   2: MIN
        
    *   3: Second Chance
        
    *   4: CLOCK
        
*   :
    
    *   20: 1 MB physical memory
        
    *   24: 16 MB physical memory
        

### Example

`   ./vmsim 1 24   `

This runs the simulator with the LRU algorithm and 24-bit physical addressing (16 MB).

📊 Output
---------

*   **Console Output**: Displays statistics including hits, misses, page faults, hit ratio, and miss ratio
    
*   **Gnuplot Graph**: Static plot of memory access traces saved as plot.txt and displayed using Gnuplot
    
*   **SDL2 Window**:
    
    *   Bar graph of performance metrics (hits, misses, page faults)
        
    *   Animation of physical memory frames with a status bar showing step, page, and hit/miss status
        

### Controls in SDL2 Visualization

*   **Space**: Start autoplay or step through memory accesses in manual mode
    
*   **P**: Toggle between auto and manual playback
    
*   **\+ / -**: Increase or decrease animation speed
    
*   **ESC**: Exit the visualization
    

📝 Notes
--------

*   The simulator collects memory traces from /proc/641/maps by default. Modify the get\_memory\_access\_trace function in the source code to target a different process ID (PID).
    
*   Root permissions may be required to access memory maps of certain processes.
    
*   Trace collection is limited to 100 unique pages and a maximum of 1,000,000 entries to prevent overflow.
    
*   Ensure the DejaVuSans font (/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf) is available for SDL2 rendering.
    

🧹 Cleanup
----------

To remove the compiled binary and generated files:

`   rm vmsim plot.txt   `

🛠️ Troubleshooting
-------------------

*   **SDL2 Errors**: Ensure libsdl2-dev and libsdl2-ttf-dev are installed and the DejaVuSans font is available.
    
*   **Gnuplot Issues**: Verify gnuplot is installed and accessible in your PATH.
    
*   **Permission Denied**: Run the program with sudo if you encounter issues accessing /proc//maps.
    
*   **No Trace Collected**: Check if the specified PID exists and has readable memory maps.
