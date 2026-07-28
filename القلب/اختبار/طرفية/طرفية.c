#include "طرفية.h"



#include "القلب/اختبار/لوحة_المفاتيح/لوحة_المفاتيح.h"
#include "القلب/مكتبة_المحركات/مكتبة_معالجة_النصوص.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_العرض/الأدارة/محرك_العرض.h"
#include "القلب/الوحدات/وحدات_الخدمات/وحدات_الشاشة/محرك_العرض/الأدارة/سطح_المكتب/مدير_النوافذ/مدير_النوافذ.h"



#include "القلب/اختبار/التخزين/نظام_الملفات/نظام_العرب.h"
#include "القلب/اختبار/التخزين/نظام_الملفات/الملفات/الملفات.h"
#include "القلب/اختبار/التخزين/نظام_الملفات/المجلدات/المجلدات.h"

#include "محرك_المقاطعات/أدارة_تنظيم_المقاطعات/مدير_الطاقة.h"

#define MAX_LINES 1000
#define MAX_LINE_LENGTH 128

static char terminal_lines[MAX_LINES][MAX_LINE_LENGTH];
static int line_count = 0;

static char input_buffer[MAX_LINE_LENGTH];
static int input_length = 0;

void terminal_print(const char *text)
{
    if (line_count >= MAX_LINES)
    {
        for (int i = 1; i < MAX_LINES; i++)
        {
            strcpy(terminal_lines[i - 1], terminal_lines[i]);
        }

        line_count = MAX_LINES - 1;
    }

    strcpy(terminal_lines[line_count], text);
    line_count++;
}


static void terminal_ls_callback(const char* name)
{
    terminal_print(name);
}


static void terminal_touch(char* name)
{
    char path[128];

    strcpy(path, "/");
    strcat(path, name);


    if (diskfs_create(path, "") )
    {
        terminal_print("File created");
    }
    else
    {
        terminal_print("Create failed");
    }
}

static void terminal_clear(void)
{
    line_count = 0;
}

static void terminal_execute_command(void)
{
    if (strcmp(input_buffer, "clear") == 0)
    {
        terminal_clear();
    }
    else if (strcmp(input_buffer, "help") == 0)
    {
        terminal_print("Commands:");
        terminal_print("help");
        terminal_print("clear");
        terminal_print("echo <text>");
        terminal_print("info");
        terminal_print("ls");
        terminal_print("touch <name>");
        terminal_print("power off");
        terminal_print("reboot>");
    }
    else if (strcmp(input_buffer, "info") == 0)
    {
        terminal_print("OS : ArabOS v4.3");
        terminal_print("kernel : Shahin 0.2");
        terminal_print("shell : ArabOS shell 0.1");
        terminal_print("                                ########");
        terminal_print("                         ######################");
        terminal_print("                     ######                  ######");
        terminal_print("                  #####                          #####");
        terminal_print("                ####                                ####");
        terminal_print("              ###                                      ###");
        terminal_print("            ####                    #######             ###");
        terminal_print("           ###                          #######           ###");
        terminal_print("          ###                              ######          ###");
        terminal_print("         ###               #                 ######         ###");
        terminal_print("        ###             #####                 #######        ##");
        terminal_print("        ##              #######         ##     #######        ##");
        terminal_print("       ###             #########     #####      #######       ##");
        terminal_print("       ##             ############ ###          #######       ###");
        terminal_print("       ##              ################         ########       ##");
        terminal_print("       ##              ################         ########       ##");
        terminal_print("       ##               ############            ########       ##");
        terminal_print("        ##                #######              #########      ##");
        terminal_print("        ##               ########             #########       ##");
        terminal_print("         ##     #        #####  ##           ##########      ##");
        terminal_print("         ###     ##      ##                 ##########      ###");
        terminal_print("          ###     ###                     ###########      ###");
        terminal_print("           ###     #####              ##############      ###");
        terminal_print("             ##      #############################       ##");
        terminal_print("              ###      #########################       ###");
        terminal_print("                ###       ##################         ###");
        terminal_print("                  ####           #####            ####");
        terminal_print("                     #####                    #####");
        terminal_print("                         ######################");
        terminal_print("                                 #######");
    }
    else if (strncmp(input_buffer, "echo ", 5) == 0)
    {
        terminal_print(input_buffer + 5);
    }

    else if (strcmp(input_buffer, "ls") == 0)
    {
        terminal_print("Files:");

        arabfs_list_path(
            0,
            terminal_ls_callback
        );
    }

    else if (strncmp(input_buffer, "touch ", 6) == 0)
    {
        terminal_touch(input_buffer + 6);
    }

    else if (strcmp(input_buffer, "power off") == 0)
    {
        terminal_print("Shutting down...");
        system_shutdown();
    }

    else if (strcmp(input_buffer, "reboot") == 0)
    {
        terminal_print("restart");
        system_reboot();
    } 
   
    else if (input_length != 0)
    {
        terminal_print("Unknown command");
    }

    input_length = 0;
    input_buffer[0] = '\0';
}

void terminal_init(void)
{
    line_count = 0;

    input_length = 0;
    input_buffer[0] = '\0';

    terminal_print("Type 'help' for commands.");
    terminal_print("");
}

void terminal_update(void)
{
    char c = keyboard_read();

    if (!c)
        return;

    if (c == '\n')
    {
        if (input_length != 0)
        {
                char command[MAX_LINE_LENGTH + 8];

                strcpy(command, "user> ");
                strcat(command, input_buffer);

                terminal_print(command);

                terminal_execute_command();
        }

        return;
    }

    if (c == '\b')
    {
        if (input_length > 0)
        {
            input_length--;
            input_buffer[input_length] = '\0';
        }

        return;
    }

    if (input_length < MAX_LINE_LENGTH - 1)
    {
        input_buffer[input_length++] = c;
        input_buffer[input_length] = '\0';
    }
}

void terminal_render(void)
{
    const window_t *window = window_get(0);

    if (!window || !window->visible)
        return;

    const int padding = 10;
    const int title_height = 40;

    const int char_width = 8;
    const int line_height = 10;

    int max_chars =
        (window->width - padding * 2) / char_width;

    if (max_chars <= 0)
        return;

    int max_visible_lines =
        (window->height - title_height - padding) / line_height;

    if (max_visible_lines <= 0)
        return;

    int y = window->y + title_height;

    int drawn_lines = 0;

    for (int i = 0; i < line_count; i++)
    {
        const char *text = terminal_lines[i];

        while (*text)
        {
            if (drawn_lines >= max_visible_lines)
                goto draw_prompt;

            char part[MAX_LINE_LENGTH];

            int len = 0;

            while (text[len] &&
                   len < max_chars)
            {
                part[len] = text[len];
                len++;
            }

            part[len] = '\0';

            draw_text(
                window->x + padding,
                y,
                part,
                0x0000FF00
            );

            y += line_height;
            drawn_lines++;

            text += len;
        }

        if (*(terminal_lines[i]) == '\0')
        {
            if (drawn_lines >= max_visible_lines)
                goto draw_prompt;

            y += line_height;
            drawn_lines++;
        }
    }

draw_prompt:

    if (drawn_lines >= max_visible_lines)
        return;

    char prompt[MAX_LINE_LENGTH + 16];

    strcpy(prompt, "user> ");
    strcat(prompt, input_buffer);

    const char *text = prompt;

    while (*text)
    {
        if (drawn_lines >= max_visible_lines)
            break;

        char part[MAX_LINE_LENGTH];

        int len = 0;

        while (text[len] &&
               len < max_chars)
        {
            part[len] = text[len];
            len++;
        }

        part[len] = '\0';

        draw_text(
            window->x + padding,
            y,
            part,
            0x00FFFF00
        );

        y += line_height;
        drawn_lines++;

        text += len;
    }
}
