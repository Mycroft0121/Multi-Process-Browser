# Multi-Process Browser
       
 1. The purpose of the program:  
 The purpose of the program is to run a web-browser with a multiprocess architecture that can open tabs and render web-pages in them. Because of the design, one tab crashing will not stall any other tab or the web-browser.
       

2. How to compile the program:  
Input the following command:  
make

3. How to use the program from the shell (syntax):
   Input the following command:  
   ./browser

4. What exactly the program does:
   It begins by initializing three arrays to track the channel structs,
   the status of the channels, and the pid of the processes piped to
   those channels. We then create a channel for the controller and add
   it to the channel array. Next we fork the controller and put its
   pid into the pid array. Then comes our main loop. We first update
   the values of our status array (indexArray) based on the presence of
   an in use channel in the channel array. It then loops through every
   index of our arrays. If a channel is in use at the current index,
   we make a non-blocking read on that pipe. We then check to see if we
   have space for an additional tab, and if so, we obtain its index.
   We then check if the possibly recaived message type matches one of the 
   three types of messages. If the message is CREATE_TAB and we have room,
   we will create a channel for it and add it to the array of channels. 
   Then we fork the tab process, run the wrapper code to create it, and
   add its pid to the pid array. If the message was TAB_KILLED, we check
   to see if it came from the controller or not. If so, we send the TAB_KILLED
   message to every open channel (each tab process). If it did not come from
   the controller, we send the TAB_KILLED message to the specified tab.
   If the message was NEW_URI_ENTERED, we pass the message to the specified
   tab. At the end of each possible message, we reset the saved message to a 
   default one. Next we try to clean up and terminated process by using waitpid.
   If a process is cleaned up, we close its pipes and update our arrays.
   Outside of the loop through all array indexes, but inside the while(1),
   we loop through every element of our pid array. We check to see if every
   process has ended, and if so, we exit our main program normally. Otherwise
   we sleep and start the while loop again.

5. Explicit assumptions we have made:
   We have assumed that the controller process will be at index 0 of
   our communication channel and pid arrays. Additionally, our tabs
   will be numbered starting from 1 up to a maximum of 10. The user
   can not have more than 10 uri tabs open at once. When creating a
   tab, the tab number of the newly opened tab will be the lowest number
   of all non-open tabs. That is, if only Tabs 1 and 3 are open, Tab 2
   will be the next to open.

6. Our strategies for error handling:
   We have checked the return value of every system call we make, as
   well as many of our own functions we have written. If the error
   should not be recoverable, we free any resources then exit.
   If we should be able to continue executing the program, we fix
   any logical problems the error caused, and we continue running.
   We also handled the invalid tab index error by printing a message
   and not trying to execute the uri update.


# Usage
go to src directory and type  
make  
./browser
