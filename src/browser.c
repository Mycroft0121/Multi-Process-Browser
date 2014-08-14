/* CSci4061 F2013 Assignment 2
* date:  10/28/13
* names: Jonathan Schworer, Yuanshun Yao
* ids:    3597293, 4445470 */

#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <gtk/gtk.h>

#define MAXTAB 11

ssize_t r_read (int fd, void *buf, size_t size);
ssize_t r_write (int fd, void *buf, size_t size);
/*
 * Name:		uri_entered_cb
 * Input arguments:'entry'-address bar where the url was entered
 *			 'data'-auxiliary data sent along with the event
 * Output arguments:void
 * Function:	When the user hits the enter after entering the url
 *			in the address bar, 'activate' event is generated
 *			for the Widget Entry, for which 'uri_entered_cb'
 *			callback is called. Controller-tab captures this event
 *			and sends the browsing request to the router(/parent)
 *			process.
 */
void
uri_entered_cb (GtkWidget * entry, gpointer data) {
    if (data == NULL) {
        return;
    }

    browser_window *b_window = (browser_window *) data;
    comm_channel channel = b_window->channel;

    // Get the tab index where the URL is to be rendered
    int tab_index = query_tab_id_for_request (entry, data);
    //handle invalid index
    if (tab_index <= 0 || tab_index > 10) {
        printf ("error: Invalid tab index entered\n");
        return;
    }

    // Get the URL.
    char *uri = get_entered_uri (entry);
    //get info from uri and write to router
    child_req_to_parent new_req;
    new_req.type = NEW_URI_ENTERED;
    new_req.req.uri_req.render_in_tab = tab_index;
    strcpy(new_req.req.uri_req.uri, uri);
    //write to router
    if(r_write(channel.child_to_parent_fd[1], &new_req, sizeof(new_req)) == -1) {
        perror("fail to write new uri message from controller to router");
        exit(-1);
    }
}

/*
 * Name:		new_tab_created_cb
 * Input arguments:	'button' - whose click generated this callback
 *			'data' - auxillary data passed along for handling
 *			this event.
 * Output arguments:    void
 * Function:		This is the callback function for the 'create_new_tab'
 *			event which is generated when the user clicks the '+'
 *			button in the controller-tab. The controller-tab
 *			redirects the request to the parent (/router) process
 *			which then creates a new child process for creating
 *			and managing this new tab.
 */
void
new_tab_created_cb (GtkButton * button, gpointer data) {
    if (data == NULL) {
        return;
    }

    int tab_index = ((browser_window *) data)->tab_index;
    //This channel have pipes to communicate with router.
    comm_channel channel = ((browser_window *) data)->channel;

    // Create a new request of type CREATE_TAB
    child_req_to_parent new_req;

    // Users press + button on the control window. 
    // Get info and write to router
    int browser_tab_index;
    browser_tab_index = tab_index;

    new_req.type = CREATE_TAB;
    new_req.req.new_tab_req.tab_index = browser_tab_index;
    //write to router
    if (r_write (channel.child_to_parent_fd[1], &new_req, sizeof (new_req)) == -1) {
        perror ("Failed to write CREATE_TAB message from CONTROLLER to ROUTER");
        exit (-1);
    }
    ((browser_window *) data)->tab_index = browser_tab_index + 1;
}

/*
 * Name:                run_control
 * Input arguments:     'comm_channel': Includes pipes to communctaion with Router process
 * Output arguments:    void
 * Function:            This function will make a CONTROLLER window and be blocked until the program terminate.
 */
int
run_control (comm_channel comm) {
    browser_window *b_window = NULL;

    //Create controler process
    if(create_browser (CONTROLLER_TAB, 0, G_CALLBACK (new_tab_created_cb), G_CALLBACK (uri_entered_cb), &b_window, comm) != 0)
      printf("error for create_browser function\n");

    //go into infinite loop.
    show_browser ();
    return 0;
}

/*
* Name:                 run_url_browser
* Input arguments:      'nTabIndex': URL-RENDERING tab index
                        'comm_channel': Includes pipes to communctaion with Router process
* Output arguments:     void
* Function:             This function will make a URL-RENDRERING tab Note.
*                       You need to use below functions to handle tab event.
*                       1. process_all_gtk_events();
                        2. process_single_gtk_event();
*/
int
run_url_browser (int nTabIndex, comm_channel comm) {
    browser_window *b_window = NULL;

    //Create controler window
    if(create_browser (URL_RENDERING_TAB, nTabIndex, G_CALLBACK (new_tab_created_cb),G_CALLBACK (uri_entered_cb), &b_window, comm) == -1)
      printf("Error for create_browser function\n");

    while (1) {
        process_single_gtk_event ();
        usleep (1000);

        child_req_to_parent blank_req;
        child_req_to_parent new_req;
        // non-block read from pipe

        if (r_read
                (comm.parent_to_child_fd[0], &new_req,
                 sizeof (child_req_to_parent)) == -1) {
            perror ("ROUTER to TAB pipe read failed");
        }
        req_type messageType;
        messageType = new_req.type;
        if (messageType == CREATE_TAB) {
            //Ignore it
        }
        
	else if(messageType == NEW_URI_ENTERED) {
	  new_uri_req urir = new_req.req.uri_req;
	  if(render_web_page_in_tab(urir.uri, b_window) == -1 ) {
	    printf("Web page rendering attempt failed\n");
	  }
	}
        
        else if (messageType == TAB_KILLED) {
            process_all_gtk_events ();
            break;
        } else {
            //Error: unrecognised message type;
        }
        new_req.type = blank_req.type;
    }
    return 0;
}

// create a comm_channel struct, 
// input are two fd arrays created by pipe
// return a comm_channel
int
makeCommChannel (int p_c_fd[], int c_p_fd[], comm_channel * cc) {
    if (sizeof (p_c_fd) / sizeof (p_c_fd[0]) != 2
            || sizeof (c_p_fd) / sizeof (c_p_fd[0]) != 2) {
        printf ("fd array size must be 2\n");
    }
    int i, flags;
    for (i = 0; i < 2; i++) {
        cc->parent_to_child_fd[i] = p_c_fd[i];
        cc->child_to_parent_fd[i] = c_p_fd[i];

        if( (flags = fcntl (cc->child_to_parent_fd[i], F_GETFL, 0)) == -1) {
	  perror("fcntrl call encountered an error");
	}
        if( fcntl (cc->child_to_parent_fd[i], F_SETFL, flags | O_NONBLOCK) == -1) {
	  perror("fcntrl call encountered an error");
        }

        if( (flags = fcntl (cc->parent_to_child_fd[i], F_GETFL, 0)) == -1) {
	  perror("fcntrl call encountered an error");
	}

        if( fcntl (cc->parent_to_child_fd[i], F_SETFL, flags | O_NONBLOCK) == -1) {
	  perror("fcntrl call encountered an error");
	}
    }
    return 0;
}

//read not interrpterd by signal
ssize_t
r_read (int fd, void *buf, size_t size) {
    ssize_t retval;
    while (retval = read (fd, buf, size), retval == -1 && errno == EINTR);
    if (retval == -1 && errno == EAGAIN) {
        //There was nothing to read from pipe
        return 0;
    }
    return retval;
}

//write nitinterrepted by signal
ssize_t
r_write (int fd, void *buf, size_t size) {
    char *bufp;
    size_t bytesToWrite;
    size_t bytesWritten;
    size_t totalBytes;
    for (bufp = buf, bytesToWrite = size, totalBytes = 0; bytesToWrite > 0; bufp += bytesWritten, bytesToWrite -= bytesWritten) {
        bytesWritten = write (fd, bufp, bytesToWrite);
        if (bytesWritten == -1 && (errno != EINTR))
            return -1;
        if (bytesWritten == -1)
            bytesWritten = 0;
        totalBytes += bytesWritten;
    }
    return totalBytes;
}

//determines if an int array of size 11 contains the element -1
int hasNegOne(int array[]) {
  int i;
  for (i = 0; i < 11; i++) {
    if (array[i] == -1) {
      return i; // Returns index of the first -1 found
    } 
  }
  return -1; //If no -1s are found, return -1;
}


int 
main () {
    //This is Router process
    //Make a controller and URL-RENDERING tab when user request it.
    //With pipes, this process should communicate with controller and tabs.
    comm_channel channelArray[MAXTAB];
    int indexArray[MAXTAB];
    int pidArray[MAXTAB];
    int z, w, y;
    //initialized to all -1
    for (z = 0; z < MAXTAB; z++) {
        for (w = 0; w < 2; w++) {
            channelArray[z].child_to_parent_fd[w] = -1;
            channelArray[z].parent_to_child_fd[w] = -1;
        }
    }
    //initialize to all -1
    for (z = 0; z < MAXTAB; z++) {
        indexArray[z] = -1;
    }
    //initialize to all -10
    for (z = 0; z < MAXTAB; z++) {
        pidArray[z] = -10;
    }
    int router_ctrler_fd[2], ctrler_router_fd[2];
    comm_channel ctrlerChannel;
    //create pipes
    if (pipe (router_ctrler_fd) == -1) {
        perror ("error for router_ctrler pipe creation");
        return -1;
    }
    if (pipe (ctrler_router_fd) == -1) {
        perror ("error for ctrler_router pipe creation");
        return -1;
    }
    //make comm_channel
    if (makeCommChannel (router_ctrler_fd, ctrler_router_fd, &ctrlerChannel) ==
            -1) {
        printf ("makeCommChannel failed\n");
        return -1;
    }
    // controller is first element in channelArray
    channelArray[0] = ctrlerChannel;
    //fork to create controller
    int pid;
    if ((pid = fork ()) < 0) {
        //If Error
        perror ("forking the controller failed");
        return -1;
    } else if (pid == 0) {
        //If Child (controller)
        int controllerStatus;
        controllerStatus = run_control (ctrlerChannel);
    } else {
        //If Parent   (router)
        pidArray[0] = pid;  //store pid
        while (1) {
            int i, k, h;
            for (i = 0; i < MAXTAB; i++) { 
                if (channelArray[i].child_to_parent_fd[0] > -1
                        && channelArray[i].parent_to_child_fd[0] > -1
                        && channelArray[i].child_to_parent_fd[1] > -1
                        && channelArray[i].parent_to_child_fd[1] > -1) {
                    indexArray[i] = i; //check is fd modified or not, if yes, then store the index into indexArray
                }
		if(channelArray[i].child_to_parent_fd[0] == -1
                        && channelArray[i].parent_to_child_fd[0] == -1
                        && channelArray[i].child_to_parent_fd[1] == -1
                        && channelArray[i].parent_to_child_fd[1] == -1) {
                    indexArray[i] = -1; // if all -1, store -1 to indexArray
		}
            }
            child_req_to_parent blank_req;
            child_req_to_parent new_req;
            // non-block read from pipe
            for (k = 0; k < MAXTAB; k++) {
                if (indexArray[k] > -1) {
		    h = indexArray[k];
                    if (r_read(channelArray[h].child_to_parent_fd[0], &new_req,sizeof (child_req_to_parent)) == -1)
                        perror ("non-block error from ctrler to router");
                    req_type messageType;
                    messageType = new_req.type;
		    int openIndex = hasNegOne(indexArray);
		    if(-1 == openIndex && messageType == CREATE_TAB) {
		      printf("The max number of open tabs has been reached.\n");
		      new_req.type = blank_req.type;
		    }
                    if (messageType == CREATE_TAB
                            && openIndex > 0) {
                        //A CREATE_TAB message was recieved from the controller.
                        //Create pipes
                        //Use pipes to make a commChannel
                        int router_tab_fd[2], tab_router_fd[2];
                        comm_channel tabChannel;
                        if (pipe (router_tab_fd) == -1) {
                            perror ("error for router_tab pipe creation");
                        }
                        if (pipe (tab_router_fd) == -1) {
                            perror ("error for tab_router pipe creation");
                        }
                        if (makeCommChannel
                                (router_tab_fd, tab_router_fd, &tabChannel) == -1) {
                            printf ("makeCommChannel failed\n");
                        }

                        //Add channel to comm list
                        channelArray[openIndex] = tabChannel;
                        new_req.type = blank_req.type;	

                        //fork()
                        int tabpid;
                        if ((tabpid = fork ()) < 0) {
                            //If Error
                            perror ("forking the tab failed");
                            return -1;
                        } else if (tabpid == 0) {
                            //If Child (tab)
                            int url_browser_status;
                            url_browser_status = run_url_browser (openIndex, tabChannel);
			    exit(0);
                        } else {
                            //If Parent (router)
			    pidArray[openIndex] = tabpid;
                        }
                    }
                    if (messageType == TAB_KILLED) {
		        if (h == 0) {  //If this message is recieved from the controller
			  //Send TAB_KILLED messages to all open comm channels (all tabs)
			  for (y=1; y < MAXTAB; y++) {
			    if (indexArray[y] > 0 ) {
			      if (r_write(channelArray[y].parent_to_child_fd[1], &new_req, sizeof (new_req)) == -1) {
				perror ("Failed to write TAB_KILLED message from ROUTER to TAB when closing all tabs b/c of controller close");
				//close all pipes and end all processes
				exit (-1);
			      }
			    }
			  }
			}
			else {//for tabs
			  int killedTab = new_req.req.killed_req.tab_index;
			  if (r_write(channelArray[killedTab].parent_to_child_fd[1], &new_req, sizeof (new_req)) == -1) {
			    perror ("Failed to write TAB_KILLED message from ROUTER to TAB. Zombie process will endure");
			    //close channel
			    if(close(channelArray[killedTab].child_to_parent_fd[0]) == -1)
			      perror("Fail to close channelArray");
			    if(close(channelArray[killedTab].child_to_parent_fd[1]) == -1)
			      perror("Fail to close channelArray");
			    if(close(channelArray[killedTab].parent_to_child_fd[0]) == -1)
			      perror("Fail to close channelArray");
			    if(close(channelArray[killedTab].parent_to_child_fd[1]) == -1)
			      perror("Fail to close channelArray");
			    //set back to -1
			    channelArray[killedTab].child_to_parent_fd[0] = -1;
			    channelArray[killedTab].child_to_parent_fd[1] = -1;
			    channelArray[killedTab].parent_to_child_fd[0] = -1;
			    channelArray[killedTab].parent_to_child_fd[1] = -1;
			    pidArray[killedTab] = -10;
			  }
			}
			new_req.type = blank_req.type;
                    }

                     if (messageType == NEW_URI_ENTERED) {//catch ENTER URI msg
		       int uriIndex = new_req.req.uri_req.render_in_tab;
		       if(uriIndex == 0) {
			 //Let it through, tab index errors are handled by callback function
		       }
		       else if(indexArray[uriIndex] > 0) {
			 if (r_write(channelArray[uriIndex].parent_to_child_fd[1], &new_req, sizeof (new_req)) == -1) {
			   perror ("Failed to write NEW_URI_ENTERED message from ROUTER to TAB");
			 }
		       }
		       else {
			 printf("error: Invalid tab index entered\n");
		       }
		       new_req.type = blank_req.type;
		     }
                }
		int status, waitTest;
		waitTest = -1;
		waitTest = waitpid(pidArray[k],&status,WNOHANG);
		if (waitTest > 0) {
                  //close channel
		  if(close(channelArray[k].child_to_parent_fd[0]) == -1)
                      perror("Fail to close channelArray");
		  if(close(channelArray[k].child_to_parent_fd[1]) == -1)
		    perror("Fail to close channelArray");
		  if(close(channelArray[k].parent_to_child_fd[0]) == -1) 
		    perror("Fail to close channelArray");
		  if(close(channelArray[k].parent_to_child_fd[1]) == -1)
		    perror("Fail to close channelArray");
                  //set back to -1
		  channelArray[k].child_to_parent_fd[0] = -1;
		  channelArray[k].child_to_parent_fd[1] = -1;
		  channelArray[k].parent_to_child_fd[0] = -1;
		  channelArray[k].parent_to_child_fd[1] = -1;

		  waitTest = -1;
		  pidArray[k] = -10;
		}
            }
            //check if all pid is set back to -10
	    int allChildrenDone = 1;
	    for(y = 0; y < MAXTAB; y++) {
	      if (pidArray[y] != -10) {
		allChildrenDone = 0;
	      }
	    }
	    if(allChildrenDone) {
	      break;
	    }
            usleep (50000);
        }
    }
    return 0;
}

