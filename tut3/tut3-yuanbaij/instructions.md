# Tutorial Exercise 3 - Locking Linked Lists 
---

### Overview

For this tutorial, you will be submitting your work on MarkUs.  One repo has been created for each student and has been populated with the starter code.  You are still welcome to work together, but each person must submit their work. Please include a comment in list_sync.c saying who you worked with.

####  Synchronizing access to a linked list

Before you do this exercise, you will probably want to read [chapter 29](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-locks-usage.pdf) of the Arpaci-Dusseau text.

The goal of this exercise is to get your feet wet writing some real synchronization code and thinking about how to synchronize access to a real data structure.

You are given the following starter code:

- Makefile
- list.h
- list.c A linked list insert with no synchronization.
- list_sync.c You will add the synchronization to this file.
- list_handoff.c An implementation of the hand-over-hand locking for insert described in the test.

####Step 1: Add synchronization to list_sync.c

Add the `pthread_mutex_lock` and `pthread_mutex_unlock` statements so that only one thread can be modifying the linked list at a time. Submit `list_sync.c` on MarkUs.

####Step 2: Run all three versions and time them

Note that if you run `./dolist` (the one compiled in the Makefile using the non-synchronized list) a few times you will not get the right number of elements in the list every time. Some of the inserts are lost.

Use the `time` program to time each version: `time ./dolist`, `time ./dosync`, `time ./handoff`.  Note the overhead costs of synchronization!  Put the output from these three commands into a file called `time.txt` and submit it on MarkUs.

####Submission instructions 

This tutorial exercise is to be submitted individually on MarkUs. However, you will start working on this exercise during the tutorial, and you are welcome to discuss and collaborate with your classmates. 

No late submissions will be accepted. You cannot use grace tokens for tutorial exercises. 

Your repository folder for this tutorial has already been created and populated with starter code on MarkUs.  You should be able to clone and complete the exercise on the teach.cs machines.  You should also be able to work on this on your own machine.

To receive marks for this assignment, your code should compile and run, and we should see that you put some effort into solving the problem.  
