#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "usb_serial.h"
#include "lcd_model.h"
#include "cab202_adc.h"

void jerry_shape();
void tom_shape();
void setup_tom();
void setup_walls();
void setup(void);
void draw_all();
void process();
void power_jerry_shape();
void GenerateCheese(int cheeseCount);
// Helper functions
void draw_timer(uint8_t x, uint8_t y, int value, colour_t colour);
void draw_double(uint8_t x, uint8_t y, int value, colour_t colour);
void draw_formatted(int x, int y, char *buffer, int buffer_size, const char *format, ...);
void usb_serial_send(char *message);
char Timerbuffer[6];
char buffer[8];
volatile int overflow_counter = 0;
volatile uint32_t overflow_counter1 = 0;
volatile uint32_t overflow_counter3 = 0;
volatile int dx, dy;
volatile int duty_cycle = 0;
int wall_step = 0;

#define FREQ (8000000.0)
#define PRESCALE0 (1024.0)
#define PRESCALE1 (256.0)
#define PRESCALE3 (8.0) //  for a Freq of 1Mhz
#define STATUE_BAR_HEIGHT 8
#define JERRY 0
#define TOM 1
#define MAX_FIRE 20
#define MAX_TRAP 5
#define MAX_MILK 1
#define MAX_CHEESE 5
#define MAX_LIVES 5
#define SQRT(x, y) sqrt(x *x + y * y)

int level = 0;
int score = 0;
int live = 5;
int cheeseCount = 0;
int trapCount = 0;
int point = 0;
int milkCount = 0;
int fireworkCount = 0;

int left_adc;
int right_adc;

int jerry_pixel[12][2];
int power_jerry_pixel[14][2];
int tom_pixel[9][2];
double cheese_pixel[MAX_CHEESE][2];

struct Jerry
{
    int x;
    int y;
};
struct Jerry jerry = {0, 9};
struct Jerry super_jerry;
double jerry_step = 1;

struct Tom
{
    double x;
    double y;
    double dir;
};

struct Tom tom = {79, 39, 0};

struct Object
{
    double x;
    double y;
    bool show;
};

struct Object cheese[MAX_CHEESE];
struct Object door;
struct Object trap[MAX_TRAP];
struct Object fire[MAX_FIRE];
struct Object milk[MAX_MILK];

int wall[4][4];
int x11, y11, x12, y12;
int x21, y21, x22, y22;
int x31, y31, x32, y32;
int x41, y41, x42, y42;

double tom_dx, tom_dy;
//double dx, dy;
double tolerance = 0.01;
double step = 0.5;

bool game_over = false;
bool game_pause = false;
bool CanJerryMove = true;
bool CanTomMove = true;
bool CanCheeseShow = true;
bool isSuperJerry = false;

/*
**  Initialise the LCD display.
*/
void new_lcd_init(uint8_t contrast)
{
    // Set up the pins connected to the LCD as outputs
    SET_OUTPUT(DDRD, SCEPIN); // Chip select -- when low, tells LCD we're sending data
    SET_OUTPUT(DDRB, RSTPIN); // Chip Reset
    SET_OUTPUT(DDRB, DCPIN);  // Data / Command selector
    SET_OUTPUT(DDRB, DINPIN); // Data input to LCD
    SET_OUTPUT(DDRF, SCKPIN); // Clock input to LCD

    CLEAR_BIT(PORTB, RSTPIN); // Reset LCD
    SET_BIT(PORTD, SCEPIN);   // Tell LCD we're not sending data.
    SET_BIT(PORTB, RSTPIN);   // Stop resetting LCD

    LCD_CMD(lcd_set_function, lcd_instr_extended);
    LCD_CMD(lcd_set_contrast, contrast);
    LCD_CMD(lcd_set_temp_coeff, 0);
    LCD_CMD(lcd_set_bias, 3);

    LCD_CMD(lcd_set_function, lcd_instr_basic);
    LCD_CMD(lcd_set_display_mode, lcd_display_normal);
    LCD_CMD(lcd_set_x_addr, 0);
    LCD_CMD(lcd_set_y_addr, 0);
}

double findDist(int x1, int y1, int x2, int y2)
{
    return SQRT((x1 - x2), (y1 - y2));
}

int PointLinesOnLine(int x, int y, int x1, int y1, int x2, int y2, double tolerance)
{
    double dist1 = findDist(x, y, x1, y1);
    double dist2 = findDist(x, y, x2, y2);
    double dist3 = findDist(x1, y1, x2, y2);
    return abs(dist3 - (dist1 + dist2)) <= tolerance;
}
/*---------------------------------------------------------------------*/

void isJerryTomCol()
{
    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (tom_pixel[j][0] == jerry_pixel[i][0] && tom_pixel[j][1] == jerry_pixel[i][1])
            {
                live--;
                jerry_shape();
                tom.x = 79;
                tom.y = 39;
            }
        }
    }
}

void isSuperJerryTomCol()
{
    for (int i = 0; i < 12; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (tom_pixel[j][0] == super_jerry.x && tom_pixel[j][1] == super_jerry.y)
            {
                point++;
                tom.x = 79;
                tom.y = 39;
            }
        }
    }
}

bool isJerryDoorCol()
{
    for (int i = 0; i < 12; i++)
    {
        if (door.show && door.x == jerry_pixel[i][0] && door.y == jerry_pixel[i][1])
        {
            return true;
        }
    }
    return false;
}

void isJerryCheeseCol()
{
    for (int j = 0; j < MAX_CHEESE; j++)
    {
        for (int i = 0; i < 12; i++)
        {
            if (cheese[j].show && (abs(cheese[j].x - jerry_pixel[i][0])) < 3 && (abs(cheese[j].y - jerry_pixel[i][1]) < 3))
            {
                cheese[j].show = false;
                score++;
                cheese[j].x = -10;
                cheese[j].y = -10;  
                cheeseCount--;  
                return;
            }
        }
    }
}

void isSuperJerryJerryCheeseCol()
{
    for (int j = 0; j < MAX_CHEESE; j++)
    {
        if (cheese[j].show && (abs(cheese[j].x - super_jerry.x)) < 3 && (abs(cheese[j].y - super_jerry.y) < 3))
        {
            cheese[j].show = false;
            score++;
            cheeseCount--;
        }
    }
}

void isJerryTrapCol()
{
    for (int j = 0; j < MAX_TRAP; j++)
    {
        for (int i = 0; i < 12; i++)
        {
            if (trap[j].show && (abs(trap[j].x - jerry_pixel[i][0])) < 3 && (abs(trap[j].y - jerry_pixel[i][1]) < 3))
            {
                trap[j].show = false;
                trap[j].x = -10;
                trap[j].y = -10;
                live--;
                trapCount--;
            }
        }
    }
}

void isJerryMilkCol()
{
    for (int j = 0; j < MAX_MILK; j++)
    {
        for (int i = 0; i < 12; i++)
        {
            if ((milk[j].show && (abs(milk[j].x - jerry_pixel[i][0])) < 3 && (abs(milk[j].y - jerry_pixel[i][1]) < 3)))
            {
                milk[j].show = false;
                milk[j].x = -10;
                milk[j].y = -10;
                milkCount--;
                isSuperJerry = true;
            }
        }
    }
}
/*---------------------------------------------------------------------*/
void tom_shape()
{
    tom_pixel[0][0] = tom.x;
    tom_pixel[0][1] = tom.y;
    tom_pixel[1][0] = tom.x + 1;
    tom_pixel[1][1] = tom.y;
    tom_pixel[2][0] = tom.x + 2;
    tom_pixel[2][1] = tom.y;
    tom_pixel[3][0] = tom.x + 3;
    tom_pixel[3][1] = tom.y;
    tom_pixel[4][0] = tom.x + 4;
    tom_pixel[4][1] = tom.y;

    tom_pixel[5][0] = tom.x + 2;
    tom_pixel[5][1] = tom.y + 1;
    tom_pixel[6][0] = tom.x + 2;
    tom_pixel[6][1] = tom.y + 2;
    tom_pixel[7][0] = tom.x + 2;
    tom_pixel[7][1] = tom.y + 3;
    tom_pixel[8][0] = tom.x + 2;
    tom_pixel[8][1] = tom.y + 4;
}

void draw_tom(void)
{
    for (int i = 0; i < 9; i++)
    {
        draw_pixel(tom_pixel[i][0], tom_pixel[i][1], 1);
    }
}

void setup_tom()
{
    tom.dir = rand() * M_PI * 2 / RAND_MAX;

    tom_dx = step * cos(tom.dir);
    tom_dy = step * sin(tom.dir);
}

void move_tom(void)
{
    int new_tx = round(tom.x + tom_dx);
    int new_ty = round(tom.y + tom_dy);
    bool bounced = false;

    if (new_tx < 1 || new_tx + 4 == LCD_X)
    {
        step = rand() / RAND_MAX * (1.5 - 0.5) + 0.5;
        setup_tom();
        bounced = true;
    }

    if (new_ty == STATUE_BAR_HEIGHT + 1 || new_ty + 5 == LCD_Y)
    {
        step = rand() / RAND_MAX * (1.5 - 0.5) + 0.5;
        setup_tom();
        bounced = true;
    }

    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (PointLinesOnLine(tom_pixel[i][0], tom_pixel[i][1], x11, y11, x12, y12, tolerance) ||
                PointLinesOnLine(tom_pixel[i][0], tom_pixel[i][1], x21, y21, x22, y22, tolerance) ||
                PointLinesOnLine(tom_pixel[i][0], tom_pixel[i][1], x31, y31, x32, y32, tolerance) ||
                PointLinesOnLine(tom_pixel[i][0], tom_pixel[i][1], x41, y41, x42, y42, tolerance))
            {
                tom.dir = rand() * M_PI * 2 / RAND_MAX;

                step = rand() / RAND_MAX * (1.5 - 0.5) + 0.5;
                tom_dx = step * cos(tom.dir);
                tom_dy = step * sin(tom.dir);
                break;
            }
        }
    }
    if (!bounced)
    {
        tom.x += tom_dx;
        tom.y += tom_dy;
    }
}

void update_tom(void)
{
    if (game_pause == false)
    {
        move_tom();
    }
    tom_shape();
}
/*---------------------------------------------------------------------*/
void jerry_shape()
{
    jerry_pixel[0][0] = jerry.x;
    jerry_pixel[0][1] = jerry.y;
    jerry_pixel[1][0] = jerry.x + 1;
    jerry_pixel[1][1] = jerry.y;
    jerry_pixel[2][0] = jerry.x + 2;
    jerry_pixel[2][1] = jerry.y;
    jerry_pixel[3][0] = jerry.x + 3;
    jerry_pixel[3][1] = jerry.y;
    jerry_pixel[4][0] = jerry.x + 4;
    jerry_pixel[4][1] = jerry.y;

    jerry_pixel[5][0] = jerry.x + 2;
    jerry_pixel[5][1] = jerry.y + 1;
    jerry_pixel[6][0] = jerry.x + 2;
    jerry_pixel[6][1] = jerry.y + 2;
    jerry_pixel[7][0] = jerry.x + 2;
    jerry_pixel[7][1] = jerry.y + 3;
    jerry_pixel[8][0] = jerry.x + 2;
    jerry_pixel[8][1] = jerry.y + 4;

    jerry_pixel[9][0] = jerry.x + 1;
    jerry_pixel[9][1] = jerry.y + 4;
    jerry_pixel[10][0] = jerry.x;
    jerry_pixel[10][1] = jerry.y + 4;
    jerry_pixel[11][0] = jerry.x;
    jerry_pixel[11][1] = jerry.y + 3;
}

void draw_jerry(void)
{
    if (isSuperJerry == false)
    {
        for (int i = 0; i < 12; i++)
        {
            draw_pixel(jerry_pixel[i][0], jerry_pixel[i][1], 1);
        }
    }
}

void update_jerry()
{
    jerry_step = (double) left_adc / 512;
    if (isSuperJerry == false)
    {
        int move = 0;
        if (BIT_IS_SET(PIND, 1)) // UP
        {
            move = 1;
            if (jerry_pixel[0][1] > STATUE_BAR_HEIGHT + 1)
            {
                for (int i = 0; i < 12; i++)
                {
                    jerry_pixel[i][1] = jerry_pixel[i][1] - 1 - jerry_step;
                }
            }
        }
        else if (BIT_IS_SET(PINB, 7)) // DOWN
        {
            move = 2;
            if (jerry_pixel[0][1] < LCD_Y - 5)
            {
                for (int i = 0; i < 12; i++)
                {
                    jerry_pixel[i][1] = jerry_pixel[i][1] + 1 + jerry_step;
                }
            }
        }
        else if (BIT_IS_SET(PIND, 0)) // RIGHT
        {
            move = 3;
            //while(BIT_IS_SET(PIND, 0)){}
            if (jerry_pixel[0][0] < LCD_X - 5)
            {
                for (int i = 0; i < 12; i++)
                {
                    jerry_pixel[i][0] = jerry_pixel[i][0] + 1 + jerry_step;
                }
            }
        }
        else if (BIT_IS_SET(PINB, 1)) // LEFT
        {
            move = 4;
            if (jerry_pixel[0][0] > 0)
            {
                for (int i = 0; i < 12; i++)
                {
                    jerry_pixel[i][0] = jerry_pixel[i][0] - 1 - jerry_step;
                }
            }
        }
        for (int i = 0; i < 12; i++)
        {
            if (PointLinesOnLine(jerry_pixel[i][0], jerry_pixel[i][1], x11, y11, x12, y12, tolerance) ||
                PointLinesOnLine(jerry_pixel[i][0], jerry_pixel[i][1], x21, y21, x22, y22, tolerance) ||
                PointLinesOnLine(jerry_pixel[i][0], jerry_pixel[i][1], x31, y31, x32, y32, tolerance) ||
                PointLinesOnLine(jerry_pixel[i][0], jerry_pixel[i][1], x41, y41, x42, y42, tolerance))
            {
                if (move == 1)
                {
                    jerry_shape();
                }
                else if (move == 2)
                {
                    if (jerry_pixel[0][1] < LCD_Y - 5)
                    {
                        jerry_shape();
                    }
                }
                else if (move == 3)
                {
                    jerry_shape();
                }
                else if (move == 4)
                {
                    jerry_shape();
                }
                break;
            }
        }
    }
}

void power_jerry_shape()
{
    if (isSuperJerry == true)
    {
        super_jerry.x = 0;
        super_jerry.y = 10;
    }
}

void draw_super_jerry()
{
    if (isSuperJerry == true)
    {
        draw_char(super_jerry.x, super_jerry.y, 'J', 1);
    }
}

void update_super_jerry()
{
    if (BIT_IS_SET(PIND, 1)) // UP
    {
        if (super_jerry.y > STATUE_BAR_HEIGHT + 1)
        {
            super_jerry.y = super_jerry.y - 1;
        }
    }
    else if (BIT_IS_SET(PINB, 7)) // DOWN
    {
        if (super_jerry.y < LCD_Y - 8)
        {
            super_jerry.y = super_jerry.y + 1;
        }
    }
    else if (BIT_IS_SET(PIND, 0)) // RIGHT
    {
        //while(BIT_IS_SET(PIND, 0)){}
        if (super_jerry.x < LCD_X - 5)
        {
            super_jerry.x = super_jerry.x + 1;
        }
    }
    else if (BIT_IS_SET(PINB, 1)) // LEFT
    {
        if (super_jerry.x > 0)
        {
            super_jerry.x = super_jerry.x - 1;
        }
    }
}

/*---------------------------------------------------------------------*/

bool CheckCheeseCollision()
{
    bool Iscollid = false;
    if (cheese[cheeseCount].x < 1 || cheese[cheeseCount].x > (LCD_X - 5))
    {
        Iscollid = true;
    }
    if (cheese[cheeseCount].y < 9 || cheese[cheeseCount].y > (LCD_Y - 5))
    {
        Iscollid = true;
    }
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            for (int k = 0; k < 5; k++)
            {
                if (PointLinesOnLine(cheese[cheeseCount].x + j, cheese[cheeseCount].y + k, x11, y11, x12, y12, tolerance) ||
                    PointLinesOnLine(cheese[cheeseCount].x + j, cheese[cheeseCount].y + k, x21, y21, x22, y22, tolerance) ||
                    PointLinesOnLine(cheese[cheeseCount].x + j, cheese[cheeseCount].y + k, x31, y31, x32, y32, tolerance) ||
                    PointLinesOnLine(cheese[cheeseCount].x + j, cheese[cheeseCount].y + k, x41, y41, x42, y42, tolerance))
                {
                    Iscollid = true;
                    break;
                }
            }
        }
    }
    for (int i = 0; i < 5; i++)
    {
        if (i != cheeseCount)
        {
            if (abs(cheese[cheeseCount].x - cheese[i].x) < 5 && abs(cheese[cheeseCount].y - cheese[i].y) < 5)
            {
                Iscollid = true;
                break;
            }
        }
    }
    return Iscollid;
}

void GenerateCheese(int cheeseCount)
{
    if (cheeseCount >= MAX_CHEESE)
    {
        return;
    }

    else
    {
        cheese[cheeseCount].x = round(rand() % (LCD_X - 6));
        cheese[cheeseCount].y = round(rand() % (LCD_X - 6));

        while (CheckCheeseCollision())
        {
            cheese[cheeseCount].x = round(rand() % (LCD_X - 5));
            cheese[cheeseCount].y = round(rand() % (LCD_X - 5));
        }
    }
    cheese[cheeseCount].show = true;
}

void draw_cheese(void)
{
    for (int i = 0; i < cheeseCount; i++)
    {
        if (cheese[i].show)
        {
            draw_char(cheese[i].x, cheese[i].y, 'c', 1);
        }
    }
}

/*---------------------------------------------------------------------*/
bool CheckDoorCollision()
{
    bool Iscollid = false;

    if (door.x < 1 || door.x > (LCD_X - 5))
    {
        Iscollid = true;
    }
    if (door.y < 9 || door.y > (LCD_Y - 5))
    {
        Iscollid = true;
    }
    for (int i = 0; i < 4; i++)
    {
        if (PointLinesOnLine(door.x, door.y, wall[i][0], wall[i][1], wall[i][2], wall[i][3], tolerance))
        {
            Iscollid = true;
            break;
        }
    }
    return Iscollid;
}

void GenerateDoor()
{
    if (door.show)
    {
        return;
    }
    else
    {
        door.x = round(rand() % (LCD_X - 5));
        door.y = round(rand() % (LCD_X - 5));
        while (CheckDoorCollision())
        {
            door.x = round(rand() % (LCD_X - 5));
            door.y = round(rand() % (LCD_X - 5));
        }
    }
    door.show = true;
}

void draw_door()
{
    if (door.show)
    {
        draw_char(door.x, door.y, 'd', 1);
    }
}
/*---------------------------------------------------------------------*/
bool CheckTrapCollision()
{
    bool Iscollid = false;

    if (trap[trapCount].x < 1 || trap[trapCount].x > (LCD_X - 5))
    {
        Iscollid = true;
    }
    if (trap[trapCount].y < 9 || trap[trapCount].y > (LCD_Y - 5))
    {
        Iscollid = true;
    }

    return Iscollid;
}

void GenerateTrap(int trapCount)
{
    if (trapCount >= MAX_TRAP)
    {
        return;
    }

    trap[trapCount].x = tom.x - 1;
    trap[trapCount].y = tom.y - 1;

    while (CheckTrapCollision())
    {
        trap[trapCount].x = tom.x - 1;
        trap[trapCount].y = tom.y - 1;
    }

    trap[trapCount].show = true;
}

void draw_trap()
{
    for (int i = 0; i < trapCount; i++)
    {
        if (trap[i].show)
        {
            draw_char(trap[i].x, trap[i].y, 'o', 1);
        }
    }
}

/*---------------------------------------------------------------------*/

void GenerateMilk(int milkCount)
{
    if (milkCount >= MAX_MILK)
    {
        return;
    }
    else
    {
        milk[milkCount].x = tom.x;
        milk[milkCount].y = tom.y;
    }
    milk[milkCount].show = true;
}

void draw_milk()
{
    for (int i = 0; i < milkCount; i++)
    {
        if (milk[i].show)
        {
            draw_char(milk[i].x, milk[i].y, 'm', 1);
        }
    }
}

/*---------------------------------------------------------------------*/
void Tryfire()
{
    for (int i = 0; i < MAX_FIRE; ++i)
    {
        for (int j = 0; j < 12; j++)
        {
            if (fire[i].show == false)
            {
                fire[i].x = jerry_pixel[j][0];
                fire[i].y = jerry_pixel[j][1];
                fire[i].show = true;
                break;
            }
        }
    }
    fireworkCount = 1;
}

void Shootfire()
{
    if (BIT_IS_SET(PINB, 0)) // UP
    {
        Tryfire();
    }
}

void UpdateFire()
{
    for (int i = 0; i < MAX_FIRE; i++)
    {
        if (fire[i].show == false)
            continue;
        int dir;

        if (rand() % 2 == 1)
        {
            if (tom.x < fire[i].x)
            {
                dir = 0;
            }
            else if (tom.x > fire[i].x)
            {
                dir = 1;
            }
            else if (tom.y < fire[i].y)
            {
                dir = 2;
            }
            else
            {
                dir = 3;
            }
        }
        else
        {
            if (tom.y < fire[i].y)
            {
                dir = 2;
            }
            else if (tom.y > fire[i].y)
            {
                dir = 3;
            }
            if (tom.x < fire[i].x)
            {
                dir = 0;
            }
            else
            {
                dir = 1;
            }
        }

        switch (dir)
        {
        case 0:
            fire[i].x = fire[i].x - 1;
            break;

        case 1:
            fire[i].x = fire[i].x + 1;
            break;

        case 2:
            fire[i].y = fire[i].y - 1;
            break;

        case 3:
            fire[i].y = fire[i].y + 1;
            break;
        }

        // firework disappear when it is collided with wall
        for (int j = 0; j < 4; j++)
        {
            if (PointLinesOnLine(fire[i].x, fire[i].y, x11, y11, x12, y12, tolerance) ||
                PointLinesOnLine(fire[i].x, fire[i].y, x21, y21, x22, y22, tolerance) ||
                PointLinesOnLine(fire[i].x, fire[i].y, x31, y31, x32, y32, tolerance) ||
                PointLinesOnLine(fire[i].x, fire[i].y, x41, y41, x42, y42, tolerance) ||
                fire[i].x <= 0 || fire[i].x > LCD_X - 1 ||
                fire[i].y <= 0 || fire[i].y > LCD_Y - 1)
            {
                fire[i].show = false;
                fireworkCount = 0;
            }
        }

        // If Tom hit by fireworks, reset Tom location as initial location and increase one score
        for (int j = 0; j < 9; j++)
        {
            if (fire[i].x == tom_pixel[j][0] && fire[i].y == tom_pixel[j][1])
            {
                fire[i].show = false;
                fireworkCount = 0;
                tom.x = 79;
                tom.y = 39;
            }
        }
    }
}

void draw_firework()
{
    for (int i = 0; i < MAX_FIRE; i++)
    {
        if (fire[i].show)
        {
            draw_pixel(fire[i].x, fire[i].y, 1);
        }
    }
}

/*---------------------------------------------------------------------*/
void pause_game()
{
    if (BIT_IS_SET(PINF, 5))
    {
        while (BIT_IS_SET(PINF, 5))
        {

        }
        game_pause = !game_pause;
    }
}
/*---------------------------------------------------------------------*/

void get_usb(void)
{
    if (usb_serial_available())
    {
        char tx_buffer[600];
        int c = usb_serial_getchar(); //read usb port

        if (c == 'a')
        { //
            if (isSuperJerry == false)
            {
                if (jerry_pixel[0][0] > 0)
                {
                    for (int i = 0; i < 12; i++)
                    {
                        jerry_pixel[i][0] = jerry_pixel[i][0] - 1;
                    }
                }
            }
            if (isSuperJerry == true)
            {
                super_jerry.x = super_jerry.x - 1;
            }
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'd')
        { //
            if (isSuperJerry == false)
            {
                if (jerry_pixel[0][0] < LCD_X - 5)
                {
                    for (int i = 0; i < 12; i++)
                    {
                        jerry_pixel[i][0] = jerry_pixel[i][0] + 1;
                    }
                }
            }
            if (isSuperJerry == true)
            {
                super_jerry.x = super_jerry.x + 1;
            }
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'w')
        { //
            if (isSuperJerry == false)
            {
                if (jerry_pixel[0][1] > STATUE_BAR_HEIGHT + 1)
                {
                    for (int i = 0; i < 12; i++)
                    {
                        jerry_pixel[i][1] = jerry_pixel[i][1] - 1;
                    }
                }
            }
            if (isSuperJerry == true)
            {
                super_jerry.y = super_jerry.y - 1;
            }
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 's')
        { //
            if (isSuperJerry == false)
            {
                if (jerry_pixel[0][1] < LCD_Y - 5)
                {
                    for (int i = 0; i < 12; i++)
                    {
                        jerry_pixel[i][1] = jerry_pixel[i][1] + 1;
                    }
                }
            }
            if (isSuperJerry == true)
            {
                super_jerry.y = super_jerry.y + 1;
            }
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'f')
        { //
            while (c != 'f')
            {

            }
            if (score >= 3)
            {
                Tryfire();
            }
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'l')
        { //
            level++;
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'p')
        { //
            game_pause = !game_pause;
            snprintf(tx_buffer, sizeof(tx_buffer), "received '%c'\r\n", c);
            usb_serial_send(tx_buffer);
        }

        if (c == 'i')
        { //
            int time = (overflow_counter * 256.0 + TCNT0) * PRESCALE0 / FREQ;
            int min, sec;
            sec = time % 60;
            min = time / 60 % 60;
            snprintf(tx_buffer, sizeof(tx_buffer), " Time: '%02d:%02d'\r\n Current level '%d'\r\n Current lives '%d'\r\n Current score '%d'\r\n Current firework num '%d'\r\n Current mousetraps num '%d'\r\n Current cheese num '%d'\r\n Number of Cheese has been consumed '%d'\r\n Is super-mode? '%s'\r\n Is pause-mode? '%s'\r\n", min, sec, level, live, score, fireworkCount, trapCount, cheeseCount, score, isSuperJerry ? "true" : "false", game_pause ? "true" : "false");
            usb_serial_send(tx_buffer);
        }
    }
}
/*---------------------------------------------------------------------*/
int dx1, dy1;
int dxa1, dya1;

void setup_wall1()
{
    
    wall[0][0] = 18;
    wall[0][1] = 15;
    wall[0][2] = 13;
    wall[0][3] = 25;
    //
    wall[1][0] = 25;
    wall[1][1] = 35;
    wall[1][2] = 25;
    wall[1][3] = 45;
    //
    wall[2][0] = 45;
    wall[2][1] = 10;
    wall[2][2] = 60;
    wall[2][3] = 10;
    //
    wall[3][0] = 58;
    wall[3][1] = 25;
    wall[3][2] = 72;
    wall[3][3] = 30;

    x11=wall[0][0];
    y11=wall[0][1];
    x12=wall[0][2];
    y12=wall[0][3];
    //
    x21=wall[1][0];
    y21=wall[1][1];
    x22=wall[1][2];
    y22=wall[1][3];
    //
    x31=wall[2][0];
    y31=wall[2][1];
    x32=wall[2][2];
    y32=wall[2][3];
    //
    x41=wall[3][0];
    y41=wall[3][1];
    x42=wall[3][2];
    y42=wall[3][3];
    
}

void setup_wall2()
{
    if (level == 2){
    wall[0][0] = 15;
    wall[0][1] = 15;
    wall[0][2] = 18;
    wall[0][3] = 25;
    //
    wall[1][0] = 35;
    wall[1][1] = 35;
    wall[1][2] = 25;
    wall[1][3] = 45;
    //
    wall[2][0] = 40;
    wall[2][1] = 11;
    wall[2][2] = 60;
    wall[2][3] = 13;
    //
    wall[3][0] = 58;
    wall[3][1] = 30;
    wall[3][2] = 58;
    wall[3][3] = 35;

    x11=wall[0][0];
    y11=wall[0][1];
    x12=wall[0][2];
    y12=wall[0][3];
    //
    x21=wall[1][0];
    y21=wall[1][1];
    x22=wall[1][2];
    y22=wall[1][3];
    //
    x31=wall[2][0];
    y31=wall[2][1];
    x32=wall[2][2];
    y32=wall[2][3];
    //
    x41=wall[3][0];
    y41=wall[3][1];
    x42=wall[3][2];
    y42=wall[3][3];
    }
}

void move_wall()
{
    if (game_pause == false)
    {

        //First line
        x11 += wall_step;
        if (x11 < 0 || x11 > LCD_X)
        {
            x11=wall[0][0];
            y11=wall[0][1];
            x12=wall[0][2];
            y12=wall[0][3];
        }
        y11 += wall_step;
        if (y11 < 0 || y11 > LCD_Y)
        {
            x11=wall[0][0];
            y11=wall[0][1];
            x12=wall[0][2];
            y12=wall[0][3];
        }
        x12 += wall_step;
        if (x12 < 0 || x12 > LCD_X)
        {
            x11=wall[0][0];
            y11=wall[0][1];
            x12=wall[0][2];
            y12=wall[0][3];
        }
        y12 += wall_step;
        if (y12 < 0 || y12 > LCD_Y)
        {
            x11=wall[0][0];
            y11=wall[0][1];
            x12=wall[0][2];
            y12=wall[0][3];
        }

        //2
        x21 += wall_step;
        if (x21 < 0 || x21 > LCD_X)
        {
            x21=wall[1][0];
            y21=wall[1][1];
            x22=wall[1][2];
            y22=wall[1][3];
        }
        x22 += wall_step;
        if (x22 < 0 || x22 > LCD_X)
        {
            x21=wall[1][0];
            y21=wall[1][1];
            x22=wall[1][2];
            y22=wall[1][3];
        }

        //3
        y31 += wall_step;
        if (y31 < 0 || y31 > LCD_Y)
        {
            x31=wall[2][0];
            y31=wall[2][1];
            x32=wall[2][2];
            y32=wall[2][3];
        }
        y32 += wall_step;
        if (y32 < 0 || y32 > LCD_Y)
        {
            x31=wall[2][0];
            y31=wall[2][1];
            x32=wall[2][2];
            y32=wall[2][3];
        }

        //4
        x41 -= wall_step;
        if (x41 < 0 || x41 > LCD_X)
        {
            x41=wall[3][0];
            y41=wall[3][1];
            x42=wall[3][2];
            y42=wall[3][3];
        }
        y41 += wall_step;
        if (y41 < 0 || y41 > LCD_Y)
        {
            x41=wall[3][0];
            y41=wall[3][1];
            x42=wall[3][2];
            y42=wall[3][3];
        }
        x42 -= wall_step;
        if (x42 < 0 || x42 > LCD_X)
        {
            x41=wall[3][0];
            y41=wall[3][1];
            x42=wall[3][2];
            y42=wall[3][3];
        }
        y42 += wall_step;
        if (y42 < 0 || y42 > LCD_Y)
        {
            x41=wall[3][0];
            y41=wall[3][1];
            x42=wall[3][2];
            y42=wall[3][3];
        }

        /*
        //First line
        x11 = (wall[0][0] + dx + wall_step);
        if (x11 < 0 || x11 > LCD_X)
        {
            dx = 0;
            dy = 0;
        }
        y11 = (wall[0][1] + dy + wall_step);
        if (y11 < 0 || y11 > LCD_Y)
        {
            dy = 0;
            dx = 0;
        }
        x12 = (wall[0][2] + dx + wall_step);
        if (x12 < 0 || x12 > LCD_X)
        {
            dx = 0;
            dy = 0;
        }
        y12 = (wall[0][3] + dy + wall_step);

        //second line
        x21 = (wall[1][0] + dx + wall_step) % LCD_X;
        y21 = wall[1][1] + wall_step;
        x22 = (wall[1][2] + dx + wall_step) % LCD_X;
        y22 = wall[1][3] + wall_step;
        //third line
        x31 = wall[2][0]+ wall_step;
        y31 = (wall[2][1] + dy+ wall_step) % LCD_Y;
        x32 = wall[2][2]+ wall_step;
        y32 = (wall[2][3] + dy+ wall_step) % LCD_Y;
        //fourth line
        x41 = (wall[3][0] - dx- wall_step) % LCD_X;
        y41 = wall[3][1]+ wall_step;
        x42 = (wall[3][2] - dx - wall_step) % LCD_X;
        y42 = wall[3][3]+ wall_step;
        */
    }
    
}

void draw_wall()
{
    /*
    dxa1 = (dxa1 + 1) % (LCD_X - 8);
    dya1 = (dya1 + 1) % (LCD_Y - 8);

    for (int i = 0; i < 4; i++)
    {
        draw_line(wall[i][0] + dxa1, wall[i][1] + dya1, wall[i][2] + dxa1, wall[i][3] + dya1, 1);
    }*/
    draw_line(x11, y11, x12, y12, FG_COLOUR);
    draw_line(x21, y21, x22, y22, FG_COLOUR);
    draw_line(x31, y31, x32, y32, FG_COLOUR);
    draw_line(x41, y41, x42, y42, FG_COLOUR);
}
/*---------------------------------------------------------------------*/
void SetStartScreen()
{
    draw_string(0, 9, "Daehan Jung", 1);
    draw_string(0, 18, "N10242074", 1);
    draw_string(0, 27, "Tom & Jerry", 1);
    show_screen();
    while (!BIT_IS_SET(PINF, 5))
    {

    }
    clear_screen();
    level++;

}

void GotoNextLevel()
{
    if (BIT_IS_SET(PINF, 6))
    {
        while (BIT_IS_SET(PINF, 6))
        {

        }
        level++;
        score = 0;
        live = 5;
        cheeseCount = 0;
        trapCount = 0;
    }
}
/*---------------------------------------------------------------------*/
void setup_timer0(void)
{
    // Timer0
    CLEAR_BIT(TCCR0B, WGM02);

    SET_BIT(TCCR0B, CS02);
    CLEAR_BIT(TCCR0B, CS01);
    SET_BIT(TCCR0B, CS00);

    //Enabling the timer overflow interrupt
    SET_BIT(TIMSK0, TOIE0);
}

void setup_timer1(void)
{
    CLEAR_BIT(TCCR1B, WGM12);
    CLEAR_BIT(TCCR1B, WGM13);

    // Enable timer overflow for Timer 1.
    SET_BIT(TCCR1B, CS12);
    CLEAR_BIT(TCCR1B, CS11);
    CLEAR_BIT(TCCR1B, CS10);

    // Turn on interrupts.
    SET_BIT(TIMSK1, TOIE1);
}

void setup_timer3(void)
{

    // Timer 1 in normal mode (WGM12. WGM13),
    // with pre-scaler 8 ==> 	(CS12,CS11,CS10).
    // Timer overflow on. (TOIE1)

    CLEAR_BIT(TCCR3B, WGM32);
    CLEAR_BIT(TCCR3B, WGM33);

    CLEAR_BIT(TCCR3B, CS32); //0 see table 14-6
    SET_BIT(TCCR3B, CS31);   //1
    SET_BIT(TCCR3B, CS30); //0

    //enabling the timer overflow interrupt
    SET_BIT(TIMSK3, TOIE3);
}

ISR(TIMER0_OVF_vect)
{

    if (game_pause == false)
    {
        if (level >= 1)
        {
            overflow_counter++;
        }
    }

}

ISR(TIMER1_OVF_vect)
{
    if (game_pause == false)
    {
        if (level >= 1)
        {
            overflow_counter1++;

            if (cheeseCount < MAX_CHEESE)
            {
                GenerateCheese(cheeseCount);
                cheeseCount++;
            }

            if (overflow_counter1 % 2)
            {
                if (trapCount < MAX_TRAP)
                {
                    GenerateTrap(trapCount);
                    trapCount++;
                }
            }

            if (level >= 2)
            {
                if (milkCount < MAX_MILK && isSuperJerry == false)
                {
                    if (overflow_counter1 > 3)
                    {
                        GenerateMilk(milkCount);
                        milkCount++;
                        overflow_counter1 = 0;
                    }
                }

                if (isSuperJerry == true)
                {   

	                SET_BIT(PORTB, 2);
                    SET_BIT(PORTB, 3);

                    if (overflow_counter1 > 6)
                    {
                        isSuperJerry = false;

                        jerry_shape();
                        CLEAR_BIT(PORTB, 3);
                        CLEAR_BIT(PORTB, 2);
                        overflow_counter1 = 0;
                    }
                }
            }
        }
    }
}

ISR(TIMER3_OVF_vect)
{
    if (game_pause == false)
    {
    overflow_counter3++;
    /*
    if (overflow_counter3 > 10)
    {
        dx = (dx + 1);
        dy = (dy + 1);
        overflow_counter3 = 0;
    }
    }*/
    move_wall();
}
}

/*---------------------------------------------------------------------*/

void setup(void)
{
    set_clock_speed(CPU_8MHz);
    new_lcd_init(LCD_DEFAULT_CONTRAST);
    lcd_clear();

    // Joystics and buttons
    SET_BIT(DDRB, 2);   // LED 0
    SET_BIT(DDRB, 3);   // LED 1
    CLEAR_BIT(DDRB, 0); // JOYSTICK CENTER
    CLEAR_BIT(DDRB, 1); // JOYSTICK LEFT
    CLEAR_BIT(DDRB, 7); // JOYSTICK DOWN

    CLEAR_BIT(DDRD, 0); // JOYSTICK RIGHT
    CLEAR_BIT(DDRD, 1); // JOYSTICK UP

    CLEAR_BIT(DDRF, 6); // BUTTON LEFT
    CLEAR_BIT(DDRF, 5); // BUTTON RIGHT
    SET_BIT(DDRB, 2);

    setup_timer0();
    setup_timer1();
    setup_timer3();

    // Enable timer overflow, and turn on interrupts.
    sei();

    adc_init();
    usb_init();
    while (!usb_configured())
    {
        // Block until USB is ready.
    }

    //setup_jerry();
    jerry_shape();
    power_jerry_shape();
    tom_shape();
    setup_tom();
    

    setup_wall1();
    //setup_wall2();
    level = 0;
    score = 0;
    live = 5;
    cheeseCount = 0;
    trapCount = 0;
    point = 0;
    milkCount = 0;
    fireworkCount = 0;

    if (level == 2)
    {
        
        score = 0;
        live = 5;
        cheeseCount = 0;
        trapCount = 0;
        point = 0;
        milkCount = 0;
    }

    for (int i = 0; i < MAX_CHEESE; ++i)
    {
        cheese[i].show = false;
    }
    for (int i = 0; i < MAX_TRAP; ++i)
    {
        trap[i].show = false;
    }

    door.show = false;
    game_pause = false;
}

void draw_all()
{
    int time = (overflow_counter * 256.0 + TCNT0) * PRESCALE0 / FREQ;

    draw_string(0, 0, "LV:", 1);
    draw_double(15, 0, level, 1);
    draw_string(21, 0, "L:", 1);
    draw_double(32, 0, live, 1);
    draw_string(39, 0, "S:", 1);
    draw_double(49, 0, score, 1);
    draw_timer(56, 0, time, 1);
    draw_line(0, STATUE_BAR_HEIGHT, 84, STATUE_BAR_HEIGHT, 1);

    
    //move_wall();
    draw_jerry();
    draw_tom();
    draw_cheese();
    draw_door();
    draw_trap();
    draw_firework();
    draw_milk();
    draw_super_jerry();
    draw_wall();
}

void process()
{
    left_adc = adc_read(0);
	right_adc = adc_read(1);
//
    //int jerry_adc = 
    update_jerry();
    update_tom();
    if (score >= MAX_CHEESE)
    {
        GenerateDoor();
    }
    if (isJerryDoorCol())
    {
        door.show = false;
        level++;
        score = 0;
        live = 5;
        cheeseCount = 0;
        trapCount = 0;  
    }
    if (score >= 3)
    {
        Shootfire();
    }
    GotoNextLevel();

    if (level >= 2)
    {
        get_usb();
    }
    if (isSuperJerry == true)
    {
        for (int i = 0; i < 12; i++)
        {
            jerry_pixel[i][0] = -1;
            jerry_pixel[i][1] = -1;
        }
        update_super_jerry();
        isSuperJerryJerryCheeseCol();
        isSuperJerryTomCol();
    }

    isJerryCheeseCol();
    isJerryTomCol();
    isJerryTrapCol();
    isJerryMilkCol();
    UpdateFire();

    pause_game();

    if (right_adc >= 750)
    {
        wall_step = 2;
    }
    else if (right_adc >= 500)
    {
        wall_step = 1;
    }
    else if (right_adc >= 250)
    {
        wall_step = -1;
    }
    else if (right_adc >= 0)
    {
        wall_step = -2;
    }

    if (live <= 0 || level >= 3)
    {
        game_over = true;
    }

    if (game_over == true)
    {
        clear_screen();
        draw_string(0, 9, "Game Over!", 1);
        if (BIT_IS_SET(PINF, 5))
        {
            while (!BIT_IS_SET(PINF, 5))
            {
            }
            game_over = false;
        }
    }
}

int main(void)
{
    setup();
    SetStartScreen();

    while (game_over == false)
    {
        clear_screen();
        draw_all();
        process();
        //_delay_ms(50);
        show_screen();
    }
    return 0;
}

/*-----------------------------------------------------------------------*/
//Helper functions
void draw_timer(uint8_t x, uint8_t y, int value, colour_t colour)
{
    int min, sec;
    sec = value % 60;
    min = value / 60 % 60;
    snprintf(Timerbuffer, sizeof(Timerbuffer), "%02d:%02d", min, sec);
    draw_string(x, y, Timerbuffer, colour);
}

void draw_double(uint8_t x, uint8_t y, int value, colour_t colour)
{
    snprintf(buffer, sizeof(buffer), "%d", value);
    draw_string(x, y, buffer, colour);
}

void usb_serial_send(char *message)
{
    // Cast to avoid "error: pointer targets in passing argument 1
    //	of 'usb_serial_write' differ in signedness"
    usb_serial_write((uint8_t *)message, strlen(message));
}

void draw_formatted(int x, int y, char *buffer, int buffer_size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    draw_string(x, y, buffer, FG_COLOUR);
}