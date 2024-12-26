# Operating_Systems_Programming

This repository showcases my programming skills in operating systems for job recruitment purposes. I am not responsible for any use of the code by others.

## Introduction
This repository contains programming assignments completed as part of an Operating Systems course. The assignments focus on core operating system concepts such as process management, interprocess communication, synchronization, scheduling, and virtual memory.

## Technologies Used
- **Languages:** C++/C
- **Platform:** Unix/Linux
- **Tools:** GCC, Makefile

## Assignments

1. **Unix Shell for Parsing, Forking, and Logical Operators**
   - Implemented a basic shell supporting command parsing, process creation, and handling logical operators like `&&`, `||`, and `;`.

2. **Unix Shell for Interprocess Communication via Pipes**
   - Extended the shell to support interprocess communication using pipes for commands like `ls | grep txt`.

3. **Implementation of a Monitor**
   - Designed a monitor mechanism to solve the producer-consumer problem, ensuring synchronization between threads.

4. **Scheduler with Signals**
   - Implemented a process scheduler using Unix signals to manage and switch between processes.

5. **Designing a Virtual Memory Manager**
   - Built a virtual memory manager simulating paging, using page replacement algorithms like FIFO and LRU.

## How to Run
Each assignment is organized into a separate folder. To compile and run the code:

1. Navigate to the assignment folder:
   ```bash
   cd AssignmentX
   ```
2. Compile the code:
   ```bash
   make
   ```
3. Run the program:
   ```bash
   ./program_name [args]
   ```

## Directory Structure
```
Operating_Systems_Programming/
|
|-- Assignment1_Shell/           # Unix Shell for Parsing, Forking, and Logical Operators
|-- Assignment2_Pipes/           # Unix Shell for Interprocess Communication via Pipes
|-- Assignment3_Monitor/         # Implementation of a Monitor
|-- Assignment4_Scheduler/       # Scheduler with Signals
|-- Assignment5_VirtualMemory/   # Designing a Virtual Memory Manager
|-- README.md                    # Repository documentation

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.
