#include "keyboard.h"
#include "paging.h"
#include "int_wrapper.h"
//#include "lib.c"
#include "syscall_handler.h"

static int read_lock[MAX_TERMINALS] = {0, 0, 0};
static int kbrd_buf_length[MAX_TERMINALS] = {0, 0, 0};
static int is_enter[MAX_TERMINALS] = {0, 0, 0};
static int is_shift[MAX_TERMINALS] = {0, 0, 0};
static int is_caps[MAX_TERMINALS] = {0, 0, 0};
static int is_ctrl[MAX_TERMINALS] = {0, 0, 0};
static int is_alt[MAX_TERMINALS] = {0, 0, 0};
static int current_terminal;


// keyboard buffer
static int8_t kbrd_buf[MAX_TERMINALS][KBRD_BUF_LENGTH];
static int8_t kbrd_read_buf[KBRD_BUF_LENGTH];

  
// int to keep track of location in keyboard buf
static uint8_t line_marker[MAX_TERMINALS] = {0, 0, 0};

/*
* terminal_open
*   DESCRIPTION: returns 0
*   INPUTS:         none
*   OUTPUTS:        none
*   RETURN VALUE:   0
*   SIDE EFFECTS:   none
*
*/
int32_t terminal_open(const uint8_t* filename){
  return 0;
}

/*
* terminal_close
*   DESCRIPTION: returns 0
*   INPUTS:         none
*   OUTPUTS:        none
*   RETURN VALUE:   0
*   SIDE EFFECTS:   none
*
*/
int32_t terminal_close(int32_t* fd){
  if(fd == NULL) return -1;
  return 0;
}

/*
* terminal_read
*   DESCRIPTION: reads first nbytes chars from keyboard buf to input buf
*   INPUTS:         uint8_t* buf: buffer to be written to
*                   int32_t nbytes: number of chars to write
*   OUTPUTS:        none
*   RETURN VALUE:   none
*   SIDE EFFECTS:   Writes to inputted buf, changes keyboard buf
*
*/
int32_t terminal_read(int32_t fd, uint8_t* buf, int32_t nbytes)
{
    // if inputted buffer is null return 1
    int writeBytes;
    if(buf == NULL)
    {
      return -1;
    }

    // initialize reader and set enter check to false
    read_lock[current_terminal] = 1;
    is_enter[current_terminal] = 0;

    

    // wait for enter input
    while(!is_enter[get_curr_pcb_term()]);

    // save flags
    cli();

    // initialze for loop variable
    int i;

    // Make sure nbytes is positive, if not make it 0
	  if(line_marker[current_terminal] - nbytes < 0)
    {
        writeBytes = 0;
    } 
    else
    {
        // set writebytes to the bytes to be written
        writeBytes = line_marker[current_terminal] - (nbytes - 1);
    }
    
    /* 
    * Set up 2 cases to read one if the number of bytes is greater than the buffer length
    * In which case, the copied buffer will be the size of the buffer length
    * And another case for when nbytes is smaller  than the buffer length
    * in which case the copied buffer will be the size of nbytes 
    */
    if(nbytes < KBRD_BUF_LENGTH)
    {
        // write the first nbytes values in the kbrd buffer into the transfer buffer
        for(i = 0; i < nbytes - 1; i++)
        {
            kbrd_read_buf[i] = kbrd_buf[current_terminal][i];
        }
        //Set the input buf to be the first nbytes in the keyboard buffer
        //and append /n to the last value (overwriting the value if its exactly the size of the buffer)
        kbrd_read_buf[nbytes - 1] = '\n';
        memcpy(buf, kbrd_read_buf, nbytes);
        memmove(kbrd_buf[current_terminal], kbrd_buf[current_terminal] + nbytes - 1, writeBytes);
        memset(kbrd_buf[current_terminal] + writeBytes, NULL_SCANCODE, nbytes);
        line_marker[current_terminal] = writeBytes;
    }
    else
    {
        // write the first nbytes values in the kbrd buffer into the transfer buffer
        for(i = 0; i < KBRD_BUF_LENGTH - 1; i++)
        {
            kbrd_read_buf[i] = kbrd_buf[current_terminal][i];
        }
        //Set the input buf to be the first nbytes in the keyboard buffer
        //and append /n to the last value (overwriting the value if its exactly the size of the buffer)
        kbrd_read_buf[KBRD_BUF_LENGTH - 1] = '\n';
        memcpy(buf, kbrd_read_buf, KBRD_BUF_LENGTH);
        memmove(kbrd_buf[current_terminal], kbrd_buf[current_terminal] + KBRD_BUF_LENGTH - 1, writeBytes);
        memset(kbrd_buf[current_terminal] + writeBytes, NULL_SCANCODE, KBRD_BUF_LENGTH);
        line_marker[current_terminal] = writeBytes;
    }
    
    // get flags back, relinquish read lock and set is_enter back to false, then return
    sti();
    read_lock[current_terminal] = 0;
    is_enter[current_terminal] = 0;
    int retval = (nbytes < KBRD_BUF_LENGTH) ? nbytes : KBRD_BUF_LENGTH;
    return retval;
}


/*
* terminal_write
*   DESCRIPTION: Writes first nbytes chars from buf to screen
*   INPUTS:         uint8_t* buf: buffer to be written
*                   int32_t nbytes: number of chars to write
*   OUTPUTS:        none
*   RETURN VALUE:   none
*   SIDE EFFECTS:   Writes to screen 
*
*/
int32_t terminal_write(int32_t fd, const uint8_t* buf, int32_t nbytes)
{
    // if inputted buffer is null, return
    if(buf == NULL)
    {
      return -1;
    }

    // initialize variable for for loop and save flags
    int i;
    cli();

    // print first nbytes characters of buffer to screen
    for(i = 0; i < nbytes; i++)
    {
      putc(buf[i], get_curr_pcb_term());
    }
  
    // retrieve flags and return
    sti();
    return nbytes;
}



/*
* kbrd_init
*   DESCRIPTION: Sets up Keyboard so it is ready to generate and send interrupts
*   INPUTS:         none
*   OUTPUTS:        none
*   RETURN VALUE:   none
*   SIDE EFFECTS:   Sends data to Keyboard to initialize it and initializes
*   misc keyboard values
*
*/
void kbrd_init() {
    // set shift, caps and ctrl check variables to false
    is_shift[current_terminal] = 0;
    is_caps[current_terminal] = 0;
    is_ctrl[current_terminal] = 0;
    is_alt[current_terminal] = 0;

    // set keyboard buffer to be completely null characters
    memset(kbrd_buf[current_terminal], NULL_SCANCODE, KBRD_BUF_LENGTH);

    current_terminal = 0;
    update_curr_term(0);

    // enable keyboard irq port
    enable_irq(KBRD_IRQ);
}


/*
* kbrd_int
*   DESCRIPTION: Handler kernel uses to respond to interrupt receieved by keyboard
*   INPUTS:             none
*   OUTPUTS:            none    
*   RETURN VALUE:       none
*   SIDE EFFECTS:       Takes appropriate action for some interrupt receieved by Keyboard, 
*   and ensures keyboard is left in proper state
*
*/
void kbrd_int() {
    // variable to save scancode
    uint8_t scancode;
    // variable to save key retrieved from conversion table
    uint8_t key;

    // check if keyboard status is active
    if(inb(KBRD_PORT) & 1)
    {
      // set scancode and key variables
      scancode = inb(KBRD_PORT_DATA);
      key = kbrdscancode[scancode];

      // set ctrl, shift and caps variables based on pressed keys
      if(scancode == LCTRL_PRESS)
      {
        is_ctrl[current_terminal] = 1;
      }
      else if(scancode == LCTRL_RELEASE)
      {
        is_ctrl[current_terminal] = 0;
      }
      else if(scancode == LSHIFT_PRESS || scancode == RSHIFT_PRESS)
      {
        is_shift[current_terminal] = 1;
      }
      else if(scancode == LSHIFT_RELEASE || scancode == RSHIFT_RELEASE)
      {
        is_shift[current_terminal] = 0;
      }
      else if(scancode == LALT_PRESS)
      {
        is_alt[current_terminal] = 1;
      }
      else if(scancode == LALT_RELEASE)
      {
        is_alt[current_terminal] = 0;
      }
      else if(scancode == CAPS_PRESS)
      {
          if(is_caps[current_terminal] == 1)
          {
            is_caps[current_terminal] = 0;
          }
          else
          {
            is_caps[current_terminal] = 1;
          }
      }
      else if(scancode == F1_SCANCODE && is_alt[current_terminal] == 1)
      {
        if(current_terminal != 0)
        {
            current_terminal = 0;
            update_curr_term(0);
            update_vidmem(0);
            // change_vidmap(0);
            
        }
      }
      else if(scancode == F2_SCANCODE && is_alt[current_terminal] == 1)
      {
        if(current_terminal != 1)
        {
            current_terminal = 1;
            update_curr_term(1);
            update_vidmem(1);
            // change_vidmap(1);
        }
      }
      else if(scancode == F3_SCANCODE && is_alt[current_terminal] == 1)
      {
        if(current_terminal != SECOND_TERMINAL_CASE)
        {
            current_terminal = SECOND_TERMINAL_CASE;
            update_curr_term(SECOND_TERMINAL_CASE);
            update_vidmem(SECOND_TERMINAL_CASE);
            // change_vidmap(2);
        }
      }
      // If enter is pressed, go to a new line, set is_enter to true and reset kbrd buffer if not being read from
      else if(scancode == ENTER_PRESS)
      {
          // Out put newline char to move to newline
          putc(key, current_terminal);

          // Set is_enter variable to true
          is_enter[current_terminal] = 1;

          // Reset kbrd buffer if not reading
          kbrd_buf_length[current_terminal] = 0;
          if(read_lock[current_terminal] == 0)
          {
              memset(kbrd_buf[current_terminal], NULL_SCANCODE, KBRD_BUF_LENGTH);
			        line_marker[current_terminal] = 0;
          }
      }
      // If backspace is pressed, call backspace helper function and remove char from keyboard buffer
      else if(scancode == BACKSPACE_PRESS)
      {
          // Make sure keyboard buffer holds values
          if(kbrd_buf_length[current_terminal] > 0)
          {
              backspace_helper(current_terminal);
              kbrd_buf[current_terminal][line_marker[current_terminal] - 1] = NULL_SCANCODE;
              kbrd_buf_length[current_terminal]--;
              line_marker[current_terminal]--;
          }
      }
      // If keyboard buffer is less than the max allowed size and the scancode is valid
      // print it to the screen and add the character to the buffer
      else if((kbrd_buf_length[current_terminal] < KBRD_BUF_LENGTH - 1) && scancode < SCANCODE_LENGTH)
      {
        // If ctrl key is pressed along with L, clear screen, reset cursor and puts the current state of buffer
        if(is_ctrl[current_terminal] == 1 && key == L_SCANCODE)
        {
          reset();
          puts(kbrd_buf[current_terminal], current_terminal);
        }
        else
        {
            // Set the key based on whether shift, caps lock or both are active
            if(is_shift[current_terminal] == 1 && is_caps[current_terminal] == 0)
            {
                key = shiftkbrdscancode[scancode];
            }
            else if(is_shift[current_terminal] == 1 && is_caps[current_terminal] == 1)
            {
                key = capsshiftkbrdscancode[scancode];
            }
            else if(is_shift[current_terminal] == 0 && is_caps[current_terminal] == 1)
            {
                key = capskbrdscancode[scancode];
            }
            // Output the key to the screen and the character to the buffer
            putc(key, current_terminal);
            kbrd_buf[current_terminal][line_marker[current_terminal]] = key;
            line_marker[current_terminal]++;
            kbrd_buf_length[current_terminal]++;
        }
      }
    }
    // Send end of interrupt signal to kbrd irq port
    send_eoi(KBRD_IRQ);
}

