/******************************************************
 * Author: Di Kong
 * Contact: dik214 at lehigh dot edu
 * Assignment: p10.c
 * Description: A simple battleship game with graphics
 *   working with two terminals
 ******************************************************/

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


// index for the struct ship arrays
#define A 0
#define B 1
#define F 2
#define S 3
#define M 4
// length of each ship
#define A_LEN 5
#define B_LEN 4
#define F_LEN 3
#define S_LEN 3
#define M_LEN 2
// mode for deploy & attack phase
#define P1 0
#define P2 1
// general consts
#define TOT_SHIP_CELL 17
#define TOT_ATK_CELL 100
#define BOARD_BEG_X 3
#define BOARD_BEG_Y 2
#define BOARD_SIZE  10
#define SHIP_COUNT  5
#define BUFFER_SIZE 128

// ship struct
struct ship {
  char type[25];
  int length;
  int y[5];
  int x[5];
  int is_sunk;
  int has_deployed;
  int num_cell_deployed;
};

// functions for each phase
void init();
void deploy();
void attack();
void wrap_up();
// helper functions for main phases
void create_board(int, int);
int do_deploy_ch(int);
int deploy_ship(char, int, int);
int deploy_p2();
int do_attack_ch(int);
int attack_cell(int, int);
int attack_p2();
int check_ship_align(struct ship*, int, int);
int win();
// print functions
void print_deploy_help();
void print_attack_help();
void print_prompt(char*);
void print_error(char*, int);
void print_board();
void print_ships_left(int);
void move_to_board(int, int, int);
// minor helper functions
struct ship* get_ship_by_ch(int, char);
struct ship* get_ship_by_coord(int, int, int);
void wprintw_center(WINDOW*, int, char*);
void fill_line(WINDOW*, int, char);
void erase_cell(int, int);
int check_border(int,int);
int intcmp (const void*, const void*);
int row_char2index(char);
char row_index2char(int);
int col_num2index(int);
int col_index2num(int);


// global vars
WINDOW* p1_board;
WINDOW* p2_board;
int infifo, outfifo;  // pipes for output & input
int player_id;
int board_h = BOARD_SIZE + 3;
int board_w = BOARD_SIZE * 2 + 4;
char board_p1[BOARD_SIZE][BOARD_SIZE];    // player's board
char board_p2[BOARD_SIZE][BOARD_SIZE];    // p2's board
char sank_p1[BOARD_SIZE][BOARD_SIZE];     // player's board as shown to p2
char sank_p2[BOARD_SIZE][BOARD_SIZE];     // p2's board as shown to player
struct ship ships_p1[SHIP_COUNT], ships_p2[SHIP_COUNT]; // ships info for each player


int main(int argc, char** argv)
{
  init();
  deploy();
  attack();
  wrap_up();

  return 0;
}

// initialization of all variables
void init()
{
  char str[60];
  int i, j, startx, starty;
  // init screen
  initscr();
  crmode();
  noecho();
  keypad(stdscr, TRUE);
  clear();
  starty = (LINES - board_h) / 2;
  startx = (COLS - board_w * 2 - 10) / 2;
  refresh();
  // create each board window and prompt string
  create_board(starty, startx);
  fill_line(stdscr, 0, '=');
  wprintw_center(stdscr, 0, " Welcome to the Battleship game! ");
  snprintf(str, 60, "**** Requires windows size of %2dx%2d or greater *****", board_w * 2 + 10, board_h + 6);
  wprintw_center(stdscr, 1, str);
  wprintw_center(stdscr, 2, "** Please relaunch the game after resizing window **");
  refresh();
  // init board array
  for(i = 0; i < BOARD_SIZE; i++) {
    for(j = 0; j < BOARD_SIZE; j++) {
      board_p1[i][j] = '.';
      board_p2[i][j] = '.';
      sank_p1[i][j] = '.';
      sank_p2[i][j] = '.';
    }
  }
  // init ships
  strcpy(ships_p1[A].type, "Aircraft Carrier");
  strcpy(ships_p1[B].type, "Battleship");
  strcpy(ships_p1[F].type, "Frigate");
  strcpy(ships_p1[S].type, "Submarine");
  strcpy(ships_p1[M].type, "Minesweeper");
  strcpy(ships_p2[A].type, "Aircraft Carrier");
  strcpy(ships_p2[B].type, "Battleship");
  strcpy(ships_p2[F].type, "Frigate");
  strcpy(ships_p2[S].type, "Submarine");
  strcpy(ships_p2[M].type, "Minesweeper");
  ships_p1[A].length = A_LEN;
  ships_p1[B].length = B_LEN;
  ships_p1[F].length = F_LEN;
  ships_p1[S].length = S_LEN;
  ships_p1[M].length = M_LEN;
  ships_p2[A].length = A_LEN;
  ships_p2[B].length = B_LEN;
  ships_p2[F].length = F_LEN;
  ships_p2[S].length = S_LEN;
  ships_p2[M].length = M_LEN;
  for(i = 0; i < SHIP_COUNT; i++) {
    ships_p1[i].is_sunk = 0;
    ships_p1[i].has_deployed = 0;
    ships_p1[i].num_cell_deployed = 0;
    ships_p2[i].is_sunk = 0;
    ships_p2[i].has_deployed = 0;
    ships_p2[i].num_cell_deployed = 0;
    for(i = 0; i < SHIP_COUNT; i++) {
      ships_p1[i].x[j] = -1;
      ships_p1[i].y[j] = -1;
      ships_p2[i].x[j] = -1;
      ships_p2[i].y[j] = -1;
    }
  }
  // open fifo for output/input
  print_prompt("Are you player 1 or player 2? ");
  char ch = getch();
  if((mkfifo("fifo1", 0666) < 0 || mkfifo("fifo2", 0666) < 0) && errno != EEXIST)
    print_error("mkfifo", errno);
  if(ch == '1') {
    player_id = 1;
    print_prompt("Waiting for player 2 to join...");
    infifo = open("./fifo2", O_RDONLY);
    outfifo = open("./fifo1", O_WRONLY);
  }
  else if(ch == '2') {
    player_id = 2;
    print_prompt("Waiting for player 1 to join...");
    outfifo = open("./fifo2", O_WRONLY);
    infifo = open("./fifo1", O_RDONLY);
  }
  else {
    print_prompt("Unrecognized input. Game will now exit.");
    getch();
    wrap_up();
  }
  if(outfifo < 0 || infifo < 0)
    print_error("open", errno);
}

// deploy phase 
void deploy()
{
  int i, ch, p1_counter = 0, p2_counter = 0, status;
  fill_line(stdscr, LINES-1, '=');
  wprintw_center(stdscr, LINES-1, " Ship deployment phase ");
  print_board();
  print_ships_left(P1);
  move_to_board(P1, BOARD_BEG_Y, BOARD_BEG_X);
  wmove(p1_board, BOARD_BEG_Y, BOARD_BEG_X);

  // loop for deploy phase
  while(p1_counter < TOT_SHIP_CELL) {
    ch = getch();
    if((status = do_deploy_ch(ch)) == -2)
      wrap_up();
    else if(status == 0)
      continue;
    else if(status < 2)
      p1_counter += status;
    print_prompt("Please wait for opponent move.");
    p2_counter += deploy_p2();
    move_to_board(P1, BOARD_BEG_Y, BOARD_BEG_X);
  }
  // sort ship coordinates for later use
  for(i = 0; i < SHIP_COUNT; i++) {
    qsort(ships_p1[i].y, ships_p1[i].length, sizeof(int), intcmp);
    qsort(ships_p1[i].x, ships_p1[i].length, sizeof(int), intcmp);
    qsort(ships_p2[i].y, ships_p2[i].length, sizeof(int), intcmp);
    qsort(ships_p2[i].x, ships_p2[i].length, sizeof(int), intcmp);
  }
  print_board();
  move_to_board(P1, BOARD_BEG_Y, BOARD_BEG_X);
  fill_line(stdscr, LINES-3, ' ');
}

// read from pipe and deploy on opponent's board
int deploy_p2()
{
  int x, x_i, y_i, i;
  int err;
  struct ship* s = 0;
  char oldval, t, y;
  char buffer[BUFFER_SIZE];
  int ret_val = 1;
  // read from pipe
  if(read(infifo, buffer, BUFFER_SIZE) < 0)
    print_error("read", errno);
  // parse into variables
  i = sscanf(buffer, "%c (%c,%d)", &t, &y, &x);
  if(i != 3)
    return 0;
  s = get_ship_by_ch(P2, t);
  x_i = col_num2index(x);
  y_i = row_char2index(y);

  // check if read values are valid
  if((oldval = board_p2[y_i][x_i]) != '.')
    ret_val = 0;
  if ((err = check_ship_align(s, y_i + BOARD_BEG_Y, x_i * 2 + BOARD_BEG_X)) < 0)
    return 0;
  else if(!err)
    return 0;
  // change corresponding ship information
  board_p2[y_i][x_i] = t;
  s->x[s->num_cell_deployed] = x_i;
  s->y[s->num_cell_deployed] = y_i;
  s->num_cell_deployed++;
  if(s->num_cell_deployed == s->length)
    s->has_deployed = 1;
  return ret_val;
}

// player control placement
int do_deploy_ch(int ch)
{
  int x, y, maxx, maxy;
  getyx(p1_board, y, x);
  getmaxyx(p1_board, maxy, maxx);
  move_to_board(P1, y, x);
  switch(ch) {
    case 'Q':
    case 'q':
      // quit
      return -2;
    case 'H':
    case 'h':
      // help message
      print_attack_help();
      return 0;
    case 'A':
    case 'B':
    case 'F':
    case 'S':
    case 'M':
    case 'a':
    case 'b':
    case 'f':
    case 's':
    case 'm':
      // place ship
      fill_line(stdscr, LINES-3, ' ');
      return deploy_ship(toupper((char)ch), y, x);
    case 'C':
    case 'c':
      // clear current cell. not working very well.
      fill_line(stdscr, LINES-3, ' ');
      erase_cell(y, x);
      print_board();
      print_ships_left(P1);
      wmove(p1_board, y, x);
      move_to_board(P1, y, x);
      return -1;
    case KEY_LEFT:
      // left movement
      if(x <= BOARD_BEG_X) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P1, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P1, y, x - 2);
      wmove(p1_board, y, x - 2);
      return 0;
    case KEY_RIGHT:
      // right movement
      if(x >= maxx - 3) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P1, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P1, y, x + 2);
      wmove(p1_board, y, x + 2);
      return 0;
    case KEY_UP:
      // up movement
      if(y <= BOARD_BEG_Y) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P1, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P1, y - 1, x);
      wmove(p1_board, y - 1, x);
      return 0;
    case KEY_DOWN:
      // down movement
      if(y >= maxy - 2) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P1, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P1, y + 1, x);
      wmove(p1_board, y + 1, x);
      return 0;
    default:
      return 0;
  }
}

// player place ship
int deploy_ship(char ch, int y, int x)
{
  int x_i = (x - BOARD_BEG_X) / 2, y_i = y - BOARD_BEG_Y;
  int err;
  struct ship* s = get_ship_by_ch(P1, ch);
  char oldval = board_p1[y_i][x_i];
  char buffer[BUFFER_SIZE];
  int ret_val = 1;
  // check cell validity
  if(oldval != '.') {
    erase_cell(y, x);
    ret_val = 2;
  }
  if ((err = check_ship_align(s, y, x)) < 0) {
    print_prompt("This ship has already been deployed on gameboard.");
    move_to_board(P1, y, x);
    return 0;
  }
  else if(!err) {
    print_prompt("You must place ships on a straight line!");
    move_to_board(P1, y, x);
    return 0;
  }
  // change ship information
  board_p1[y_i][x_i] = (char)ch;
  s->x[s->num_cell_deployed] = x_i;
  s->y[s->num_cell_deployed] = y_i;
  s->num_cell_deployed++;
  if(s->num_cell_deployed == s->length) {
    s->has_deployed = 1;
    print_ships_left(P1);
  }
  // write to pipe
  snprintf(buffer, BUFFER_SIZE, "%c (%c,%d)\n", ch, row_index2char(y_i), col_index2num(x_i));
  if(write(outfifo, buffer, strlen(buffer) + 1) < 0)
    print_error("write", errno);
  print_board();
  print_ships_left(P1);
  wmove(p1_board, y, x);
  move_to_board(P1, y, x);
  return ret_val;
}

// attack phase
void attack()
{
  int ch, counter = 0, status, w = 0;
  fill_line(stdscr, LINES-1, '=');
  wprintw_center(stdscr, LINES-1, " Attack phase ");
  print_board();
  print_ships_left(P2);
  move_to_board(P2, BOARD_BEG_Y, BOARD_BEG_X);
  wmove(p2_board, BOARD_BEG_Y, BOARD_BEG_X);
  // mail loop for attack phase
  while(((w = win()) == 0) && counter < TOT_ATK_CELL) {
    ch = getch();
    if((status = do_attack_ch(ch)) == -2)
      wrap_up();
    else if(status && attack_p2())
      counter ++;
  }
  print_board();
  fill_line(stdscr, LINES-2, ' ');
  // find a winner
  switch(w) {
    case 2:
      print_prompt("Congratulations! You beat the enemy!");
      break;
    case 3:
      print_prompt("Muahaha! All your base are belong to us!");
      break;
    case 1:
    default:
      print_prompt("We have a tie here... Good luck next time!");
      break;
  }
  getch();
}

// player control attack
int do_attack_ch(int ch)
{
  int x, y, maxx, maxy;
  getyx(p2_board, y, x);
  getmaxyx(p2_board, maxy, maxx);
  move_to_board(P2, y, x);
  switch(ch) {
    case 'Q':
    case 'q':
      // quit
      return -2;
    case 'H':
    case 'h':
      // help message
      print_attack_help();
      return 0;
    case ' ':
      // place bomb
      fill_line(stdscr, LINES-3, ' ');
      return attack_cell(y, x);
    case KEY_LEFT:
      // left movement
      if(x <= BOARD_BEG_X) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P2, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P2, y, x - 2);
      wmove(p2_board, y, x - 2);
      return 0;
    case KEY_RIGHT:
      // right movement
      if(x >= maxx - 3) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P2, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P2, y, x + 2);
      wmove(p2_board, y, x + 2);
      return 0;
    case KEY_UP:
      // up movement
      if(y <= BOARD_BEG_Y) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P2, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P2, y - 1, x);
      wmove(p2_board, y - 1, x);
      return 0;
    case KEY_DOWN:
      // down movement
      if(y >= maxy - 2) {
        print_prompt("Cannot move beyond board border.");
        move_to_board(P2, y, x);
        return 0;
      }
      fill_line(stdscr, LINES-3, ' ');
      move_to_board(P2, y + 1, x);
      wmove(p2_board, y + 1, x);
      return 0;
    default:
      return 0;
  }
}

// read from pipe and place p2 attack
int attack_p2()
{
  struct ship* s = 0;
  char buffer[BUFFER_SIZE];
  int i, x, x_i, y_i;
  char y;
  // read from pipe and parse into variables
  if(read(infifo, buffer, BUFFER_SIZE) < 0)
    print_error("read", errno);
  while(buffer[0] == '#')
    if(read(infifo, buffer, BUFFER_SIZE) < 0)
      print_error("read", errno);
  i = sscanf(buffer, "(%c,%d)",&y,&x);
  if(i != 2) {
    return -1;
  }
  x_i = col_num2index(x);
  y_i = row_char2index(y);
  // check validity
  if(sank_p1[y_i][x_i] != '.') {
    return 0;
  }
  // miss
  else if(board_p1[y_i][x_i] == '.') {
    sank_p1[y_i][x_i] = 'O';
    print_board();
    move_to_board(P1, y_i + 2, x_i * 2 + 3);
  }
  // hit
  else {
    s = get_ship_by_coord(P1,y_i,x_i);
    int len = s->length;
    int sunk = 1;
    sank_p1[y_i][x_i] = 'X';
    // sank any ship?
    if(s->x[0] == s->x[len-1]) {
      for(i = s->y[0]; i <= s->y[len-1]; i++) {
        if(sank_p1[i][s->x[0]] != 'X')
          sunk = 0;
      }
    }
    else {
      for(i = s->x[0]; i <= s->x[len-1]; i++) {
        if(sank_p1[s->y[0]][i] != 'X')
          sunk = 0;
      }
    }
    // if a ship is sunk give output and change corresponding ship state
    if(sunk) {
      char str[60];
      strcpy(str, "The opponent sank our ");
      strcat(str, s->type);
      print_prompt(str);
      s->is_sunk = 1;
    }
    print_board();
    move_to_board(P1, y_i + 2, x_i * 2 + 3);
  }
  
  return 1;
}

// player place attak
int attack_cell(int y, int x)
{
  int i;
  int x_i = (x - BOARD_BEG_X) / 2, y_i = y - BOARD_BEG_Y;
  struct ship* s = get_ship_by_coord(P2, y, x);
  char buffer[BUFFER_SIZE];
  // check validity
  if(sank_p2[y_i][x_i] != '.') {
    print_prompt("This cell has already been bombarded.");
    move_to_board(P2, y, x);
    wmove(p1_board, y, x);
    return 0;
  }
  // miss
  else if(board_p2[y_i][x_i] == '.') {
    print_prompt("You did not hit anything.");
    sank_p2[y_i][x_i] = 'O';
    print_board();
    print_ships_left(P2);
    move_to_board(P2, y, x);
    wmove(p1_board, y, x);
  }
  // hit
  else {
    s = get_ship_by_coord(P2,y_i,x_i);
    int len = s->length;
    print_prompt("You just hit an enemy's ship!");
    int sunk = 1;
    sank_p2[y_i][x_i] = 'X';
    // sank any ship?
    if(s->x[0] == s->x[len-1]) {
      for(i = s->y[0]; i <= s->y[len-1]; i++) {
        if(sank_p2[i][x_i] != 'X')
          sunk = 0;
      }
    }
    else {
      for(i = s->x[0]; i <= s->x[len-1]; i++) {
        if(sank_p2[y_i][i] != 'X')
          sunk = 0;
      }
    }
    // if a ship is sunk give output and change state
    if(sunk) {
      char str[60];
      strcpy(str, "You sank opponent's ");
      strcat(str, s->type);
      print_prompt(str);
      s->is_sunk = 1;
    }
    print_board();
    print_ships_left(P2);
    move_to_board(P2, y, x);
    wmove(p1_board, y, x);
  }
  // write to pipe
  snprintf(buffer, BUFFER_SIZE, "(%c,%d)\n", row_index2char(y_i), col_index2num(x_i));
  if(write(outfifo, buffer, strlen(buffer) + 1) < 0)
    print_error("write", errno);
  return 1;
}


// program closing
void wrap_up()
{
  close(infifo);
  close(outfifo);
  erase();
  refresh();
  endwin();

  exit(0);
}

// determine if anyone wins
int win()
{
  int i, p1_all_sunk = 1, p2_all_sunk = 1;
  for(i = 0; i < SHIP_COUNT; i++) {
    if(ships_p1[i].is_sunk <= 0)
      p1_all_sunk = 0;
    if(ships_p2[i].is_sunk <= 0)
      p2_all_sunk = 0;
  }
  if(p1_all_sunk && p2_all_sunk)
    return 1;
  else if(p2_all_sunk)
    return 2;
  else if(p1_all_sunk)
    return 3;
  return 0;
}

// move cursor to somewhere on a player's board
void move_to_board(int mode, int y, int x)
{
  int begy, begx;
  getbegyx((mode == P1 ? p1_board : p2_board), begy, begx);
  move(begy + y, begx + x);
}

// print a help message
void print_deploy_help()
{
  print_prompt("Press corresponding key to deploy, q to quit game");
}

// print a help message
void print_attack_help() 
{
  print_prompt("Press space key to attack current location, q to quit game");
}

// print some prompt on window
void print_prompt(char* msg)
{
  fill_line(stdscr, LINES-3, ' ');
  wprintw_center(stdscr, LINES-3, msg);
  refresh();
}

// print game board
void print_board() {
  int i, j;
  mvwprintw(p1_board, 1, 1, "  1 2 3 4 5 6 7 8 9 0");
  for(i = 0; i < BOARD_SIZE; i++) {
    wmove(p1_board, 2 + i, 1);
    wprintw(p1_board, "%c ", row_index2char(i));
    for(j = 0; j < BOARD_SIZE; j++) {
      wprintw(p1_board, "%c ", (sank_p1[i][j] == '.') ? board_p1[i][j] : sank_p1[i][j]);
    }
  }
  wrefresh(p1_board);
  mvwprintw(p2_board, 1, 1, "  1 2 3 4 5 6 7 8 9 0");
  for(i = 0; i < BOARD_SIZE; i++) {
    wmove(p2_board, 2 + i, 1);
    wprintw(p2_board, "%c ", row_index2char(i));
    for(j = 0; j < BOARD_SIZE; j++) {
//      switch in this line to see opponent's board
//      wprintw(p2_board, "%c ", (sank_p2[i][j] == '.') ? board_p2[i][j] : sank_p2[i][j]);
       wprintw(p2_board, "%c ", sank_p2[i][j]);
    }
  }
  wrefresh(p2_board);
}

// helper method to print which ships are left
void print_ships_left(int mode)
{
  int i;
  char str[60];
  if(mode == P1) {
    strcpy(str, "Ships left for deployment:");
    for(i = 0; i < SHIP_COUNT; i++) {
      if(!ships_p1[i].has_deployed) {
        char t[3] = {' ', ships_p1[i].type[0], '\0'};
        strcat(str, t);
      }
    }
  }
  else {
    strcpy(str, "Opponent's ships left:");
    for(i = 0; i < SHIP_COUNT; i++) {
      if(ships_p2[i].is_sunk <= 0) {
        char t[3] = {' ', ships_p2[i].type[0], '\0'};
        strcat(str, t);
      }
    }
  }
  fill_line(stdscr, LINES-2, ' ');
  wprintw_center(stdscr, LINES-2, str);
  refresh();
}

// bug out with error message
void print_error(char* error_func, int error_num) {
  close(infifo);
  close(outfifo);
  erase();
  refresh();
  endwin();
  printf("%s error: %s\n", error_func, strerror(error_num));
  exit(-1);
}

// return reference to a ship with given char
struct ship* get_ship_by_ch(int mode, char ch)
{
  struct ship* s = 0;
  switch(ch) {
    case 'A':
      if(mode == P1)
        s = &ships_p1[A];
      else
        s = &ships_p2[A];
      break;
    case 'B':
      if(mode == P1)
        s = &ships_p1[B];
      else
        s = &ships_p2[B];
      break;
    case 'F':
      if(mode == P1)
        s = &ships_p1[F];
      else
        s = &ships_p2[F];
      break;
    case 'S':
      if(mode == P1)
        s = &ships_p1[S];
      else
        s = &ships_p2[S];
      break;
    case 'M':
      if(mode == P1)
        s = &ships_p1[M];
      else
        s = &ships_p2[M];
      break;
    default:
      break;
  }
  return s;
}

// return reference to a ship with given mode and coordinate
struct ship* get_ship_by_coord(int mode, int y_i, int x_i)
{
  struct ship* s = 0;
  if (mode == P1)
    s = get_ship_by_ch(mode, board_p1[y_i][x_i]);
  else
    s = get_ship_by_ch(mode, board_p2[y_i][x_i]);
  return s;
}

// erase a cell in deployment phase, ignore for now
void erase_cell(int y, int x)
{
  int y_i = y - BOARD_BEG_Y, x_i = (x - BOARD_BEG_X) / 2;
  if(board_p1[y_i][x_i] != '.') {
    struct ship* s = get_ship_by_ch(P1, board_p1[y_i][x_i]);
    s->num_cell_deployed--;
    s->has_deployed = 0;
    board_p1[y_i][x_i] = '.';
  }
}

// create window for game board
void create_board(int starty, int startx)
{
  p1_board = newwin(board_h, board_w, starty, startx);
  box(p1_board, 0, 0);
  p2_board = newwin(board_h, board_w, starty, COLS-startx-board_w);
  box(p2_board, 0, 0);
  wprintw_center(p1_board, 0, " Player ");
  wprintw_center(p2_board, 0, " Opponent ");
  wrefresh(p1_board);
  wrefresh(p2_board);
}

// print a centered line
void wprintw_center(WINDOW* currw, int wline, char* str)
{
  int len = strlen(str);
  int maxx, maxy;
  (void) maxy;
  getmaxyx(currw, maxy, maxx);
  int startx = (maxx - len) / 2;
  mvwprintw(currw, wline, startx, str);
  wrefresh(currw);
}

// fill a line with given char
void fill_line(WINDOW* currw, int wline, char ch)
{
  int i, len, __unused y;
  getmaxyx(stdscr, y, len);
  char str[2] = {ch, '\0'};
  wmove(currw, wline, 0);
  for(i = 0; i < len; i++)
    wprintw(currw, str);
  wrefresh(currw);
}

// compare two ints. used by qsort.
int intcmp (const void * ptr1, const void * ptr2)
{
  int i1 = *((int*)ptr1);
  int i2 = *((int*)ptr2);
  return (i1 - i2);
}

// check if given x&y is aligned and adjacent with deployed cells
int check_ship_align(struct ship* s, int y, int x)
{
  int y_i = y - BOARD_BEG_Y, x_i = (x - BOARD_BEG_X) / 2;
  int num = s->num_cell_deployed;
  int x_lstep = 0, y_lstep = 0, x_rstep = 0, y_rstep = 0;
  if(s->has_deployed)
    return -1;
  if(num < 1)
    return 1;
  else if(num < 2) {
    if(y_i == s->y[0] && (x_i == s->x[0] - 1 || x_i == s->x[0] + 1))
      return 1;
    if(x_i == s->x[0] && (y_i == s->y[0] - 1 || y_i == s->y[0] + 1))
      return 1;
    return 0;
  }
  qsort(s->y, num, sizeof(int), intcmp);
  qsort(s->x, num, sizeof(int), intcmp);
  x_lstep = s->x[1] - s->x[0];
  y_lstep = s->y[1] - s->y[0];
  x_rstep = s->x[num-2] - s->x[num-1];
  y_rstep = s->y[num-2] - s->y[num-1];
  if((s->y[0] - y_i == y_lstep && s->x[0] - x_i == x_lstep) || 
     (s->y[num-1] - y_i == y_rstep && s->x[num-1] - x_i == x_rstep))
    return 1;
  return 0;
}

// convert row board letter to array index
int row_char2index(char c)
{
  return (int)(c - 'A');
}

// convert row array index to board letter
char row_index2char(int i)
{
  return (char)(i + 'A');
}

// convert column board number to array index
int col_num2index(int i)
{
  return (i == 0) ? 9 : i - 1;
}

// convert column array index to board number
int col_index2num(int i)
{
  return (i == 9) ? 0 : i + 1;
}
