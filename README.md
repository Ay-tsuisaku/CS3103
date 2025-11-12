CS3103 Operating Systems – Programming Assignment

Quantization and De-quantization with Multi-threading

----------------------------------------------------------------------------------------

This program simulates an image compression pipeline using multi-threading in C++.

It consists of three threads:

- Camera Thread: Generates frames and loads them into a FIFO cache.

- Transformer Thread: Compresses frames using quantization and de-quantization.

- Estimator Thread: Calculates Mean Squared Error (MSE) between original and compressed frames.

----------------------------------------------------------------------------------------

You can find following files:

- 58532418_58533440_58542922.cpp : Main program implementing threads and synchronization.

- compression.c : Provides compression function (quantization + de-quantization).

- generate_frame_vector.c : Generates random frames for simulation.

- Makefile : Automates compilation.

-----------------------------------------------------------------------------------------

First, to compile the program on Linux, you need to type:

$ make

-The make command will build program by compiling all three source files with g++ and linking pthread.

Second, run the program with one argument: interval in seconds for camera thread.

for example:

$ ./58532418_58533440_58542922 2

-Then you can see the expected output:

mse = 0.000541
mse = 0.000578
...
(10 lines for 10 frames)
