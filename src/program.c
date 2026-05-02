#include <stdio.h>
#include <libusb-1.0/libusb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include "xid.h"
#include <curses.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define SBC_VISUAL_WIDTH 92
#define SBC_VISUAL_HEIGHT 24

void async_api_loop(libusb_context *ctx, libusb_device_handle *dev);
void async_api_callback(struct libusb_transfer *xfer);
void print_in_data(USB_XboxGamepad_InReport_t *in_data);
void print_in_data_sb(USB_SteelBattalion_InReport_t *in_data);
void clear_screen(int max_x, int max_y);
void print_in_data_text(USB_SteelBattalion_InReport_t *in_data);
void print_in_data_graphical(USB_SteelBattalion_InReport_t *in_data);
void draw_steel_battalion_controller(int x, int y, USB_SteelBattalion_InReport_t *in_data);
void draw_left_block(int x, int y, USB_SteelBattalion_InReport_t *in_data);
void draw_center_block(int x, int y, USB_SteelBattalion_InReport_t *in_data);
void draw_right_block(int x, int y, USB_SteelBattalion_InReport_t *in_data);
void draw_pedals(int x, int y, USB_SteelBattalion_InReport_t *in_data);

USB_SteelBattalion_InReport_t in_data;
USB_SteelBattalion_OutReport_t out_data;

libusb_device_handle *dev = NULL;
bool shutting_down = false;
pthread_t led_thread;

static WINDOW *window;
static int window_width;
static int window_height;

#define MODE_TEXT 0
#define MODE_GRAPHICAL 1
static int current_mode = MODE_GRAPHICAL;

int color_variant = 2;

void *led_thread_func(void *notUsed);
void draw_table(int max_x, int max_y);
static int last_key_down = -1;

int main(int argc, char *argv[]) {

	memset(&in_data, 0, sizeof(in_data));
	memset(&out_data, 0, sizeof(out_data));
	out_data.bLength = sizeof(out_data);

	libusb_context *ctx = NULL;
	int result = libusb_init(&ctx);
	if(result) {
		return 0;
	}

	dev = libusb_open_device_with_vid_pid(ctx, CAPCOM_VID, STEEL_BATTALION_CONTROLLER_PID);

	if(dev == NULL) {
        printf("Steel Battalion controller not found\n");
    } else {
        setlocale(LC_ALL, "");
        window = initscr();
        noecho();
        cbreak();
        keypad(stdscr, TRUE);
        timeout(2);
        use_default_colors();
        start_color();

        init_pair(1, COLOR_RED, -1);
        init_pair(2, COLOR_GREEN, -1);
        init_pair(3, COLOR_BLUE, -1);
        init_pair(4, COLOR_YELLOW, -1);

		libusb_detach_kernel_driver(dev, 0);
		libusb_claim_interface(dev, 0);

		uint8_t buffer[16];

        pthread_create(&led_thread, NULL, led_thread_func, NULL);

		async_api_loop(ctx, dev);

		libusb_close(dev);
	}

    endwin();
    if(led_thread != 0)
	    pthread_join(led_thread, NULL);

	printf("Exiting Libusb\n");
	libusb_exit(ctx);
	return 0;
}

void async_api_loop(libusb_context *ctx, libusb_device_handle *dev)
{
	uint8_t *in_buffer = malloc(sizeof(in_data));
	memset(in_buffer, 0, sizeof(in_data));
	struct libusb_transfer *in_transfer = libusb_alloc_transfer(0);
	libusb_fill_interrupt_transfer(in_transfer, dev, ENDPOINT_IN, in_buffer, sizeof(in_data), async_api_callback, NULL, INTERRUPT_TIMEOUT);
	int send_response = libusb_submit_transfer(in_transfer);

	while(!shutting_down)
    {
		libusb_handle_events(ctx);
	}
}

void async_api_callback(struct libusb_transfer *xfer) {
	if(xfer->endpoint == ENDPOINT_IN) {
		memcpy(&in_data, xfer->buffer, xfer->actual_length);
		print_in_data_sb(&in_data);

		struct libusb_transfer *new_xfer = libusb_alloc_transfer(0);
		uint8_t *in_buffer = malloc(sizeof(in_data));
		memset(in_buffer, 0, sizeof(in_data));
		libusb_fill_interrupt_transfer(new_xfer, xfer->dev_handle, ENDPOINT_IN, in_buffer, sizeof(in_data), async_api_callback, NULL, INTERRUPT_TIMEOUT);
		libusb_submit_transfer(new_xfer);
	}
    else if(xfer->endpoint == ENDPOINT_OUT) {
    }
	free(xfer->buffer);
	libusb_free_transfer(xfer);
}

float sq_len(int16_t x, int16_t y) {
	float fx = x / 32768.0f;
	float fy = y / 32768.0f;
	return (fx * fx) + (fy * fy);
}

void print_in_data_sb(USB_SteelBattalion_InReport_t *in_data)
{
    if(current_mode == MODE_TEXT)
        print_in_data_text(in_data);
    else
        print_in_data_graphical(in_data);
}

static void process_user_input()
{
    int user_input = getch();
    if(user_input != ERR)
    {
        last_key_down = user_input;
        if(user_input == 265) { 			// F1
            current_mode = MODE_TEXT;
        } else if(user_input == 266) {		// F2
            current_mode = MODE_GRAPHICAL;
        } else if(user_input == 267) {      // F3
            if(color_variant == 2)
                color_variant = 3;
            else
                color_variant = 2;
        } else if(user_input == 27) {		// ESC
            shutting_down = true;
        }
    }
}

void print_in_data_text(USB_SteelBattalion_InReport_t *in_data)
{
    char buffer[64];
    memset(buffer, 0, sizeof(buffer));
    char *const gears[] = {
        "R", "N", NULL, "1", "2", "3", "4", "5"
    };
    const int rows = 19;

    process_user_input();

    int max_x = getmaxx(window) - 1;
    int max_y = getmaxy(window);
    if (max_x != window_width ||
        max_y != window_height) {
        clear_screen(max_x + 1, max_y);
    }

    draw_table(max_x, max_y);

	// Draw Left Block
    if(in_data->gearLever == 0)
        snprintf(buffer, 32, "Gear:   ");
    else
        snprintf(buffer, 32, "Gear: %24s", gears[2 + in_data->gearLever]);
    mvaddstr(3, 2, buffer);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, 32, "Rotation Lever: %14d", in_data->rotationLever);
    mvaddstr(5, 2, buffer);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, 32, "Sight Change: %8d, %6d", in_data->sightChangeX, in_data->sightChangeY);
    mvaddstr(6, 2, buffer);

    snprintf(buffer, 32, "Reset Sight Change: %10s", in_data->dButtons.SightChange ? "Pressed" : "Released");
    mvaddstr(7, 2, buffer);

    snprintf(buffer, 32, "Filt Control System: %9s", in_data->dButtons.ToggleFiltControl ? "ON" : "OFF");
    mvaddstr(9, 2, buffer);
    snprintf(buffer, 32, "Oxygen Supply System: %8s", in_data->dButtons.ToggleOxygenSupply ? "ON" : "OFF");
    mvaddstr(10, 2, buffer);
    snprintf(buffer, 32, "Fuel Flow Rate: %14s", in_data->dButtons.ToggleFuelFlowRate ? "ON" : "OFF");
    mvaddstr(11, 2, buffer);
    snprintf(buffer, 32, "Buffer Material: %13s", in_data->dButtons.ToggleBufferMaterial ? "ON" : "OFF");
    mvaddstr(12, 2, buffer);
    snprintf(buffer, 32, "VT Location Measurement: %5s", in_data->dButtons.ToggleVTLocation ? "ON" : "OFF");
    mvaddstr(13, 2, buffer);

    // Draw Center Block
    snprintf(buffer, 32, "Comm1: %8s", in_data->dButtons.Comm1 ? "Pressed" : "Released");
    mvaddstr(3, 35, buffer);
    snprintf(buffer, 32, "Comm2: %8s", in_data->dButtons.Comm2 ? "Pressed" : "Released");
    mvaddstr(4, 35, buffer);
    snprintf(buffer, 32, "Comm3: %8s", in_data->dButtons.Comm3 ? "Pressed" : "Released");
    mvaddstr(5, 35, buffer);
    snprintf(buffer, 32, "Comm4: %8s", in_data->dButtons.Comm4 ? "Pressed" : "Released");
    mvaddstr(6, 35, buffer);
    snprintf(buffer, 32, "Comm5: %8s", in_data->dButtons.Comm5 ? "Pressed" : "Released");
    mvaddstr(7, 35, buffer);

    snprintf(buffer, 32, "Tuner: %8d", in_data->tunerDial);
    mvaddstr(10, 35, buffer);

    snprintf(buffer, 32, "F1: %23s", in_data->dButtons.Function1 ? "Pressed" : "Released");
    mvaddstr(3, 53, buffer);
    snprintf(buffer, 32, "F2: %23s", in_data->dButtons.Function2 ? "Pressed" : "Released");
    mvaddstr(4, 53, buffer);
    snprintf(buffer, 32, "F3: %23s", in_data->dButtons.Function3 ? "Pressed" : "Released");
    mvaddstr(5, 53, buffer);

    snprintf(buffer, 32, "Tank Detach: %14s", in_data->dButtons.TankDetach ? "Pressed" : "Released");
    mvaddstr(6, 53, buffer);
    snprintf(buffer, 32, "Override: %17s", in_data->dButtons.Override ? "Pressed" : "Released");
    mvaddstr(7, 53, buffer);
    snprintf(buffer, 32, "Night Scope: %14s", in_data->dButtons.NightScope ? "Pressed" : "Released");
    mvaddstr(8, 53, buffer);

    snprintf(buffer, 32, "F.S.S.: %19s", in_data->dButtons.ForecastShootingSystem ? "Pressed" : "Released");
    mvaddstr(9, 53, buffer);
    snprintf(buffer, 32, "Manipulator: %14s", in_data->dButtons.Manipulator ? "Pressed" : "Released");
    mvaddstr(10, 53, buffer);
    snprintf(buffer, 32, "Line Color Change: %8s", in_data->dButtons.LineColorChange ? "Pressed" : "Released");
    mvaddstr(11, 53, buffer);

    snprintf(buffer, 64, "Washing: %36s", in_data->dButtons.Washing ? "Pressed" : "Released");
    mvaddstr(13, 35, buffer);

    snprintf(buffer, 64, "Extinguisher: %31s", in_data->dButtons.Extinguisher ? "Pressed" : "Released");
    mvaddstr(14, 35, buffer);

    snprintf(buffer, 64, "Chaff: %38s", in_data->dButtons.Chaff ? "Pressed" : "Released");
    mvaddstr(15, 35, buffer);

    snprintf(buffer, 64, "Main: %39s", in_data->dButtons.WeaponConMain ? "Pressed" : "Released");
    mvaddstr(16, 35, buffer);

    snprintf(buffer, 64, "Sub: %40s", in_data->dButtons.WeaponConSub ? "Pressed" : "Released");
    mvaddstr(17, 35, buffer);

    snprintf(buffer, 64, "Magazine Change: %28s", in_data->dButtons.WeaponConMagazine ? "Pressed" : "Released");
    mvaddstr(18, 35, buffer);

    // Draw Right Block
    snprintf(buffer, 64, "Aiming Lever: %13d, %6d", in_data->aimingX, in_data->aimingY);
    mvaddstr(3, 83, buffer);

    snprintf(buffer, 64, "Lock On: %26s", in_data->dButtons.LockOn ? "Pressed" : "Released");
    mvaddstr(4, 83, buffer);

    snprintf(buffer, 64, "Main Weapon: %22s", in_data->dButtons.MainWeapon ? "Pressed" : "Released");
    mvaddstr(5, 83, buffer);

    snprintf(buffer, 64, "Sub Weapon: %23s", in_data->dButtons.Fire ? "Pressed" : "Released");
    mvaddstr(6, 83, buffer);

    snprintf(buffer, 64, "Multi Monitor:");
    mvaddstr(8, 83, buffer);

    snprintf(buffer, 64, "  Open/Close: %21s", in_data->dButtons.MultiMonitorOpenClose ? "Pressed" : "Released");
    mvaddstr(9, 83, buffer);

    snprintf(buffer, 64, "  Map Zoom In/Out: %16s", in_data->dButtons.MultiMonitorMapZoomInOut ? "Pressed" : "Released");
    mvaddstr(10, 83, buffer);

    snprintf(buffer, 64, "  Mode Select: %20s", in_data->dButtons.MultiMonitorModeSelect ? "Pressed" : "Released");
    mvaddstr(11, 83, buffer);

    snprintf(buffer, 64, "  Sub Monitor Mode Select: %8s", in_data->dButtons.MultiMonitorSubMonitor ? "Pressed" : "Released");
    mvaddstr(12, 83, buffer);

    snprintf(buffer, 64, "Main Monitor:");
    mvaddstr(14, 83, buffer);

    snprintf(buffer, 64, "  Zoom In: %24s", in_data->dButtons.MainMonitorZoomIn ? "Pressed" : "Released");
    mvaddstr(15, 83, buffer);

    snprintf(buffer, 64, "  Zoom Out: %23s", in_data->dButtons.MainMonitorZoomOut ? "Pressed" : "Released");
    mvaddstr(16, 83, buffer);

    snprintf(buffer, 64, "Eject: %28s", in_data->dButtons.Eject ? "Pressed" : "Released");
    mvaddstr(18, 83, buffer);

    snprintf(buffer, 64, "Cockpit Hatch: %20s", in_data->dButtons.CockpitHatch ? "Pressed" : "Released");
    mvaddstr(19, 83, buffer);

    snprintf(buffer, 64, "Ignition: %25s", in_data->dButtons.Ignition ? "Pressed" : "Released");
    mvaddstr(20, 83, buffer);

    snprintf(buffer, 64, "Start: %28s", in_data->dButtons.Start ? "Pressed" : "Released");
    mvaddstr(21, 83, buffer);

    // Draw Pedals
    snprintf(buffer, 64, "Slide Step: %18d", in_data->leftPedal);
    mvaddstr(max_y - 2, 2, buffer);
    snprintf(buffer, 64, "Brake: %38d", in_data->middlePedal);
    mvaddstr(max_y - 2, 35, buffer);
    snprintf(buffer, 64, "Accelerator: %22d", in_data->rightPedal);
    mvaddstr(max_y - 2, 83, buffer);
    move(0,0);
}

void print_in_data_graphical(USB_SteelBattalion_InReport_t *in_data)
{
    process_user_input();

    int max_x = getmaxx(window) - 1;
    int max_y = getmaxy(window);
    if (max_x != window_width ||
        max_y != window_height) {
        clear_screen(max_x + 1, max_y);
    }
    draw_steel_battalion_controller((max_x - SBC_VISUAL_WIDTH) / 2, (max_y - SBC_VISUAL_HEIGHT) / 2, in_data);
    move(0,0);

    refresh();
}

void *led_thread_func(void *notUsed) {
    while(!shutting_down)
    {
        if(out_data.EmergencyExit > 0) out_data.EmergencyExit--;
        if(out_data.CockpitHatch > 0) out_data.CockpitHatch--;
        if(out_data.Ignition > 0) out_data.Ignition--;
        if(out_data.Start > 0) out_data.Start--;
        if(out_data.OpenClose > 0) out_data.OpenClose--;
        if(out_data.MapZoomInOut > 0) out_data.MapZoomInOut--;
        if(out_data.ModeSelect > 0) out_data.ModeSelect--;
        if(out_data.SubMonitorModeSelect > 0) out_data.SubMonitorModeSelect--;
        if(out_data.MainMonitorZoomIn > 0) out_data.MainMonitorZoomIn--;
        if(out_data.MainMonitorZoomOut > 0) out_data.MainMonitorZoomOut--;
        if(out_data.ForecastShootingSystem > 0) out_data.ForecastShootingSystem--;
        if(out_data.Manipulator > 0) out_data.Manipulator--;
        if(out_data.LineColorChange > 0) out_data.LineColorChange--;
        if(out_data.Washing > 0) out_data.Washing--;
        if(out_data.Extinguisher > 0) out_data.Extinguisher--;
        if(out_data.Chaff > 0) out_data.Chaff--;
        if(out_data.TankDetach > 0) out_data.TankDetach--;
        if(out_data.Override > 0) out_data.Override--;
        if(out_data.NightScope > 0) out_data.NightScope--;
        if(out_data.FunctionF1 > 0) out_data.FunctionF1--;
        if(out_data.FunctionF2 > 0) out_data.FunctionF2--;
        if(out_data.FunctionF3 > 0) out_data.FunctionF3--;
        if(out_data.MainWeaponControl > 0) out_data.MainWeaponControl--;
        if(out_data.SubWeaponControl > 0) out_data.SubWeaponControl--;
        if(out_data.MagazineChange > 0) out_data.MagazineChange--;
        if(out_data.Comm1 > 0) out_data.Comm1--;
        if(out_data.Comm2 > 0)out_data.Comm2--;
        if(out_data.Comm3 > 0)out_data.Comm3--;
        if(out_data.Comm4 > 0)out_data.Comm4--;
        if(out_data.Comm5 > 0) out_data.Comm5--;

        out_data.GearR = (in_data.gearLever == -2) ? 15 : 0;
        out_data.GearN = (in_data.gearLever == -1) ? 15 : 0;
        out_data.Gear1 = (in_data.gearLever == 1) ? 15 : 0;
        out_data.Gear2 = (in_data.gearLever == 2) ? 15 : 0;
        out_data.Gear3 = (in_data.gearLever == 3) ? 15 : 0;
        out_data.Gear4 = (in_data.gearLever == 4) ? 15 : 0;
        out_data.Gear5 = (in_data.gearLever == 5) ? 15 : 0;

        if(in_data.dButtons.Eject) out_data.EmergencyExit = 15;
        if(in_data.dButtons.CockpitHatch) out_data.CockpitHatch = 15;
        if(in_data.dButtons.Ignition) out_data.Ignition = 15;
        if(in_data.dButtons.Start) out_data.Start = 15;
        if(in_data.dButtons.MultiMonitorOpenClose) out_data.OpenClose = 15;
        if(in_data.dButtons.MultiMonitorMapZoomInOut) out_data.MapZoomInOut = 15;
        if(in_data.dButtons.MultiMonitorModeSelect) out_data.ModeSelect = 15;
        if(in_data.dButtons.MultiMonitorSubMonitor) out_data.SubMonitorModeSelect = 15;
        if(in_data.dButtons.Comm1) out_data.Comm1 = 15;
        if(in_data.dButtons.Comm2) out_data.Comm2 = 15;
        if(in_data.dButtons.Comm3) out_data.Comm3 = 15;
        if(in_data.dButtons.Comm4) out_data.Comm4 = 15;
        if(in_data.dButtons.Comm5) out_data.Comm5 = 15;
        if(in_data.dButtons.MainMonitorZoomIn) out_data.MainMonitorZoomIn = 15;
        if(in_data.dButtons.MainMonitorZoomOut) out_data.MainMonitorZoomOut = 15;
        if(in_data.dButtons.ForecastShootingSystem) out_data.ForecastShootingSystem = 15;
        if(in_data.dButtons.Manipulator) out_data.Manipulator = 15;
        if(in_data.dButtons.LineColorChange) out_data.LineColorChange = 15;
        if(in_data.dButtons.Washing) out_data.Washing = 15;
        if(in_data.dButtons.Extinguisher) out_data.Extinguisher = 15;
        if(in_data.dButtons.Chaff) out_data.Chaff = 15;
        if(in_data.dButtons.TankDetach) out_data.TankDetach = 15;
        if(in_data.dButtons.Override) out_data.Override = 15;
        if(in_data.dButtons.NightScope) out_data.NightScope = 15;
        if(in_data.dButtons.Function1) out_data.FunctionF1 = 15;
        if(in_data.dButtons.Function2) out_data.FunctionF2 = 15;
        if(in_data.dButtons.Function3) out_data.FunctionF3 = 15;
        if(in_data.dButtons.WeaponConMain) out_data.MainWeaponControl = 15;
        if(in_data.dButtons.WeaponConSub) out_data.SubWeaponControl = 15;
        if(in_data.dButtons.WeaponConMagazine) out_data.MagazineChange = 15;

        struct libusb_transfer *new_xfer = libusb_alloc_transfer(0);
        uint8_t *out_buffer = malloc(sizeof(out_data));
        memcpy(out_buffer, &out_data, sizeof(out_data));
        libusb_fill_interrupt_transfer(new_xfer, dev, ENDPOINT_OUT, out_buffer, sizeof(out_data), async_api_callback, NULL, INTERRUPT_TIMEOUT);
        libusb_submit_transfer(new_xfer);

        usleep(20000);
    }
}

static void draw_rowsep(int row, int colsep_1, int colsep_2, int max_x)
{
    mvaddch(row, 0, ACS_LTEE);
    for(auto i = 1; i < max_x; i++)
        mvaddch(row, i, ACS_HLINE);
    mvaddch(row, max_x, ACS_RTEE);
    mvaddch(row, colsep_1, ACS_PLUS);
    mvaddch(row, colsep_2, ACS_PLUS);
}

static void draw_row(int row, int colsep_1, int colsep_2, int max_x)
{
    mvaddch(row, 0, ACS_VLINE);
    mvaddch(row, colsep_1, ACS_VLINE);
    mvaddch(row, colsep_2, ACS_VLINE);
    mvaddch(row, max_x, ACS_VLINE);
}

void clear_screen(int max_x, int max_y)
{
    for(int j = 0; j < max_y; j++)
    for(int i = 0; i < max_x; i++)
    {
        mvaddch(j, i, ' ');
    }
}

void draw_table(int max_x, int max_y)
{
    const int rows = max_y - 8;
    const int colsep_1 = 33;
    const int colsep_2 = 81;

    // Blank the Screen
    clear_screen(max_x, max_y);

    // Draw Top Line
    mvaddch(0, 0, ACS_ULCORNER);
    for(int i = 1; i < max_x; i++)
        mvaddch(0, i, ACS_HLINE);
    mvaddch(0, max_x, ACS_URCORNER);
    mvaddch(0, colsep_1, ACS_TTEE);
    mvaddch(0, colsep_2, ACS_TTEE);

    // Draw First Header Row
    draw_row(1, colsep_1, colsep_2, max_x);
    mvaddstr(1, 2, "LEFT BLOCK");
    mvaddstr(1, colsep_1 + 2, "CENTER BLOCK");
    mvaddstr(1, colsep_2 + 2, "RIGHT BLOCK");

    // Draw Row Separator
    draw_rowsep(2, colsep_1, colsep_2, max_x);

    // Draw First Data Row
    for(auto i = 0; i < rows; i++)
        draw_row(3 + i, colsep_1, colsep_2, max_x);

    // Draw Row Separator
    draw_rowsep(3 + rows, colsep_1, colsep_2, max_x);
    mvaddch(3 + rows, colsep_1, ACS_BTEE);
    mvaddch(3 + rows, colsep_2, ACS_BTEE);

    // Draw Pedals Header Row
    draw_row(4 + rows, colsep_1, colsep_2, max_x);
    mvaddstr(4 + rows, 2, "PEDALS");
    mvaddch(4 + rows, colsep_1, ' ');
    mvaddch(4 + rows, colsep_2, ' ');

    // Draw Row Separator
    draw_rowsep(5 + rows, colsep_1, colsep_2, max_x);
    mvaddch(5 + rows, colsep_1, ACS_TTEE);
    mvaddch(5 + rows, colsep_2, ACS_TTEE);

    // Draw Pedal Data Row
    draw_row(6 + rows, colsep_1, colsep_2, max_x);

    // Draw Last Line
    mvaddch(7 + rows, 0, ACS_LLCORNER);
    for(auto i = 1; i < max_x; i++)
        mvaddch(7 + rows, i, ACS_HLINE);
    mvaddch(7 + rows, max_x, ACS_LRCORNER);
    mvaddch(7 + rows, colsep_1, ACS_BTEE);
    mvaddch(7 + rows, colsep_2, ACS_BTEE);
}

void draw_shifter(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
	const char *selected = "▩";
//	const char *selected = "▣";
//    const char *unselected = "▧ ";
    const char *unselected = "□";

    x += 2;
    mvaddstr(y-1, x, "  ▁▁▁▁▁▁▁🯭");
    mvaddstr(  y, x, "  ╭─╮    │");
    mvaddstr(y+1, x, "┌─│ │  𜳵 ");
    mvaddstr(y+2, x, "├ │ │  𜳴 ");
    mvaddstr(y+3, x, "├ │ │  𜳳 ");
    mvaddstr(y+4, x, "├ │ │  𜳲 ");
    mvaddstr(y+5, x, "├ │ │  𜳱 ");
    mvaddstr(y+6, x, "├─│ │  𜳣 ");
    mvaddstr(y+7, x, "└─│ │  𜳧 ");
    mvaddstr(y+8, x, "  ╰─╯");

    // Draw Gear Lights
    attron(COLOR_PAIR(color_variant));
    mvaddstr(y+1, x+9, in_data->gearLever == 5 ? selected : unselected);
    mvaddstr(y+2, x+9, in_data->gearLever == 4 ? selected : unselected);
    mvaddstr(y+3, x+9, in_data->gearLever == 3 ? selected : unselected);
    mvaddstr(y+4, x+9, in_data->gearLever == 2 ? selected : unselected);
    mvaddstr(y+5, x+9, in_data->gearLever == 1 ? selected : unselected);
    mvaddstr(y+6, x+9, in_data->gearLever == -1 ? selected : unselected);
    attroff(COLOR_PAIR(color_variant));
    attron(COLOR_PAIR(1));
    mvaddstr(y+7, x+9, in_data->gearLever == -2 ? selected : unselected);
    attroff(COLOR_PAIR(1));

    // Draw Shift Lever
    x -= 2;
    y += 4 - in_data->gearLever;
    if(in_data->gearLever > 0)
        y++;
	else if(in_data->gearLever == 0)
        return;

    mvaddstr(  y, x, "╭───────╮");
    mvaddstr(y+1, x, "│       │");
    mvaddstr(y+2, x, "╰───────╯");

    if(in_data->gearLever < 4)
        mvaddstr(y, x+2, "┴");
    if(in_data->gearLever < 5)
        mvaddstr(y, x+4, "┴─┴");

    if(in_data->gearLever > 0)
        mvaddstr(y+2, x+2, "┬");
    if(in_data->gearLever > -2)
        mvaddstr(y+2, x+4, "┬─┬");
}

void draw_rotation_lever(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(y-1, x-2,  "▁▁▁▁▁▁▁▁▁▁▁");
    mvaddstr(  y, x-3, "──🯓┌─────┐");
    mvaddstr(y+2,   x,    "└─────┘");
    mvaddstr(y+3, x-2,  "▔▔▔▔▔▔▔▔▔▔▔");
    mvaddstr(y+1, x-2, "╍━╸");
    mvaddstr(y+1, x+6, "╺━╍");

    int offset_x;
    if(in_data->rotationLever < -24000)
        offset_x = -3;
    else if(in_data->rotationLever < -16000)
        offset_x = -2;
    else if(in_data->rotationLever < -8000)
        offset_x = -1;
    else if(in_data->rotationLever < 8000)
        offset_x = 0;
    else if(in_data->rotationLever < 16000)
        offset_x = 1;
    else if(in_data->rotationLever < 24000)
        offset_x = 2;
    else
        offset_x = 3;

    mvaddstr(y+1, x + offset_x + 3, "■");
    //mvaddstr(y+1, x + offset_x + 3, "⊕ ");
    //mvaddstr(y+1, x + offset_x + 3, "⯒");
}

void draw_toggle_switch(int x, int y, bool on)
{
	//const char *str_on = "󱨥";
	//const char *str_off = "󱨦";
	const char *str_on = "❨⬤ ❩";
	const char *str_off = "❨ ⬤❩";
	//const char *str_on = "🭵🬃 🭰";
	//const char *str_off = "🭵 🬇🭰";
	mvaddstr(y, x, on ? str_on : str_off);
}

void draw_toggle_switches(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    draw_toggle_switch(x, y+2, in_data->dButtons.ToggleFiltControl);
    draw_toggle_switch(x+2, y+1, in_data->dButtons.ToggleOxygenSupply);
    draw_toggle_switch(x+4, y+2, in_data->dButtons.ToggleFuelFlowRate);
    draw_toggle_switch(x+6, y+1, in_data->dButtons.ToggleBufferMaterial);
    draw_toggle_switch(x+8, y, in_data->dButtons.ToggleVTLocation);
}

void draw_left_block(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(   y, x, "╭─────────────────────────────╮");
    mvaddstr( y+1, x, "│                             │");
    mvaddstr( y+2, x, "│                             │");
    mvaddstr( y+3, x, "│                             │");
    mvaddstr( y+4, x, "│                             │");
    mvaddstr( y+5, x, "│                             │");
    mvaddstr( y+6, x, "│                             │");
    mvaddstr( y+7, x, "│                             │");
    mvaddstr( y+8, x, "│                             │");
    mvaddstr( y+9, x, "│                             │");
    mvaddstr(y+10, x, "│                             │");
    mvaddstr(y+11, x, "╰─────────────────────────────╯");

    mvaddstr(y-3, x+18, "🮣───🮢");
    mvaddstr(y-2, x+18, "│   │");
    mvaddstr(y-1, x+18, "🮡───🮠");

    int offset_x = 0, offset_y = 0;
    const char *upper_left_quadrant = "▘";
    const char *lower_left_quadrant = "▖";
    const char *upper_right_quadrant = "▝";
    const char *lower_right_quadrant = "▗";
    const char *left_half = "🯦";
    const char *right_half = "🯧";
    const char *top_half = "🯤";
    const char *bottom_half = "🯥";
    const char *center = "■";
    const char *symbol = center;

    if(in_data->sightChangeX < -24000)
        offset_x = -2;
    else if(in_data->sightChangeX < -12000)
        offset_x = -1;
    else if(in_data->sightChangeX < 12000)
        offset_x = 0;
    else if(in_data->sightChangeX < 24000)
        offset_x = 1;
    else
        offset_x = 2;

    if(in_data->sightChangeY < -12000)
        offset_y = -1;
    else if(in_data->sightChangeY < 12000)
        offset_y = 0;
    else
        offset_y = 1;

	if(offset_x == -2 && offset_y == -1)
    {
        symbol = upper_left_quadrant;
        offset_x = -1;
    } else if(offset_x == -1 && offset_y == -1)
    {
        symbol = upper_right_quadrant;
        offset_x = -1;
    } else if(offset_x == 0 && offset_y == -1)
    {
        symbol = top_half;
        offset_x = 0;
    } else if(offset_x == 1 && offset_y == -1)
    {
        symbol = upper_left_quadrant;
        offset_x = 1;
    } else if(offset_x == 2 && offset_y == -1)
    {
        symbol = upper_right_quadrant;
        offset_x = 1;
    } else if(offset_x == -2 && offset_y == 0)
    {
        symbol = left_half;
        offset_x = -1;
    } else if(offset_x == -1 && offset_y == 0)
    {
        symbol = right_half;
        offset_x = -1;
    } else if(offset_x == 0 && offset_y == 0)
    {
        symbol = center;
    } else if(offset_x == 1 && offset_y == 0)
    {
        symbol = left_half;
        offset_x = 1;
    } else if(offset_x == 2 && offset_y == 0)
    {
        symbol = right_half;
        offset_x = 1;
    } else if(offset_x == -2 && offset_y == 1)
    {
        symbol = lower_left_quadrant;
        offset_x = -1;
    } else if(offset_x == -1 && offset_y == 1)
    {
        symbol = lower_right_quadrant;
        offset_x = -1;
    } else if(offset_x == 0 && offset_y == 1)
    {
        symbol = bottom_half;
        offset_x = 0;
    } else if(offset_x == 1 && offset_y == 1)
    {
        symbol = lower_left_quadrant;
        offset_x = 1;
    } else if(offset_x == 2 && offset_y == 1)
    {
        symbol = lower_right_quadrant;
        offset_x = 1;
    }

    if(in_data->dButtons.SightChange)
        attron(COLOR_PAIR(color_variant));
    mvaddstr(y - 2, x + 20 + offset_x, symbol);
    attroff(COLOR_PAIR(color_variant));

	draw_shifter(x, y + 2, in_data);
    draw_toggle_switches(x + 16, y + 8, in_data);
    draw_rotation_lever(x + 17, y + 2, in_data);
}

void draw_comm_buttons(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    const char *pressed = "█";
    const char *released = "▒";
//    const char *released = "▒";
//    const char *released = "󰝣 ";

    mvaddstr(y, x, "┌─┬─┬─┬─┐┐");
    attron(COLOR_PAIR(1));
    mvaddstr(y+1, x, in_data->dButtons.Comm1 ? pressed : released);
    mvaddstr(y+1, x+2, in_data->dButtons.Comm2 ? pressed : released);
    mvaddstr(y+1, x+4, in_data->dButtons.Comm3 ? pressed : released);
    mvaddstr(y+1, x+6, in_data->dButtons.Comm4 ? pressed : released);
    mvaddstr(y+1, x+8, in_data->dButtons.Comm5 ? pressed : released);
    attroff(COLOR_PAIR(1));
}

void draw_function_buttons(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    const char *pressed = "█";
    const char *released = "▒";
//    const char *released = "▒";
//    const char *released = "󰝣 ";

    mvaddstr(  y, x+2, "┌");
    mvaddstr(y+1, x+2, "│");
    mvaddstr(y+2, x+2, "├");
    mvaddstr(y+3, x+2, "│");
    mvaddstr(y+4, x, "──┴");

    attron(COLOR_PAIR(color_variant));
    mvaddstr(  y, x+3, in_data->dButtons.Function1 ? pressed : released);
    mvaddstr(  y, x+5, in_data->dButtons.TankDetach ? pressed : released);
    mvaddstr(  y, x+7, in_data->dButtons.ForecastShootingSystem ? pressed : released);
    mvaddstr(y+2, x+3, in_data->dButtons.Function2 ? pressed : released);
    mvaddstr(y+2, x+5, in_data->dButtons.Override ? pressed : released);
    mvaddstr(y+2, x+7, in_data->dButtons.Manipulator ? pressed : released);
    mvaddstr(y+4, x+3, in_data->dButtons.Function3 ? pressed : released);
    mvaddstr(y+4, x+5, in_data->dButtons.NightScope ? pressed : released);
    mvaddstr(y+4, x+7, in_data->dButtons.LineColorChange ? pressed : released);
    attroff(COLOR_PAIR(color_variant));
}

void draw_wide_button(int x, int y, bool pressed)
{
    const char *released_str =  "🮏🮏🮏🮏";
    const char *pressed_str = "▄▄▄▄";

    mvaddstr(y, x, pressed ? pressed_str : released_str);
}

void draw_tuner_dial(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(  y, x, "╭─────╮");
    mvaddstr(y+1, x, "│     │");
    mvaddstr(y+2, x, "│     │");
    mvaddstr(y+3, x, "│     │");
    mvaddstr(y+4, x, "╰─────╯");

//    char buffer[3];
    attron(COLOR_PAIR(1));
    switch(in_data->tunerDial)
    {
        case 0:
            mvaddstr(y+2, x+1, "🬃");
            break;
        case 1:
            mvaddstr(y+1, x+1, "🬏");
            break;
        case 2:
            mvaddstr(y+1, x+1, "🬀");
            break;
        case 3:
            mvaddstr(y+1, x+2, "🬀");
            break;
        case 4:
            mvaddstr(y+1, x+3, "🯤");
            break;
        case 5:
            mvaddstr(y+1, x+4, "🬁");
            break;
        case 6:
            mvaddstr(y+1, x+5, "🬁");
            break;
        case 7:
            mvaddstr(y+1, x+5, "🬞");
            break;
        case 8:
            mvaddstr(y+2, x+5, "🬇");
            break;
        case 9:
            mvaddstr(y+3, x+5, "🬁");
            break;
        case 10:
            mvaddstr(y+3, x+5, "🬞");
            break;
        case 11:
            mvaddstr(y+3, x+4, "🬞");
            break;
        case 12:
            mvaddstr(y+3, x+3, "🯥");
            break;
        case 13:
            mvaddstr(y+3, x+2, "🬏");
            break;
        case 14:
            mvaddstr(y+3, x+1, "🬏");
            break;
        case 15:
            mvaddstr(y+3, x+1, "🬀");
            break;
    }
    attroff(COLOR_PAIR(1));
//    mvaddstr(y+1, x+1, buffer);
}

void draw_center_block(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(   y, x, "╭────────────────────────╮");
    mvaddstr( y+1, x, "│                        │");
    mvaddstr( y+2, x, "│                        │");
    mvaddstr( y+3, x, "│                        │");
    mvaddstr( y+4, x, "│                        │");
    mvaddstr( y+5, x, "│                        │");
    mvaddstr( y+6, x, "│                        │");
    mvaddstr( y+7, x, "│                        │");
    mvaddstr( y+8, x, "│  ┌─ ──── ──── ──── ─┐  │");
    mvaddstr( y+9, x, "│  ╵                  ╵  │");
    mvaddstr(y+10, x, "│ 🭺🭺🭺    🭺🭺           ╵  │");
    mvaddstr(y+11, x, "│     └────────────┘ ─┘  │");
    mvaddstr(y+12, x, "╰────────────────────────╯");

    draw_comm_buttons(x+3, y+1, in_data);
    draw_function_buttons(x + 15, y+2, in_data);
    draw_tuner_dial(x+4, y+3, in_data);

	attron(COLOR_PAIR(color_variant));
    draw_wide_button(x+5, y+9, in_data->dButtons.Washing);
    draw_wide_button(x+11, y+9, in_data->dButtons.Extinguisher);
    draw_wide_button(x+17, y+9, in_data->dButtons.Chaff);
    draw_wide_button(x+5, y+10, in_data->dButtons.WeaponConMain);
    draw_wide_button(x+11, y+10, in_data->dButtons.WeaponConSub);
    draw_wide_button(x+17, y+10, in_data->dButtons.WeaponConMagazine);
    attroff(COLOR_PAIR(color_variant));
}

void draw_big_button(int x, int y, bool btn_down)
{
    const char *pressed = "███";
    const char *released = "▒▒▒"; //𜱃𜱃𜱃";

    mvaddstr(y, x, btn_down ? pressed : released);
}

void draw_aiming_lever(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(y,   x, "🮣───╹───🮢");
    mvaddstr(y+1, x, "│       │🯑▔▔▔");
    mvaddstr(y+2, x, "─       ─");
    mvaddstr(y+3, x, "│       │");
    mvaddstr(y+4, x, "🮡───╻───🮠");

    const char *center = "󰝤";
    const char *lower_half = "▄";
    const char *upper_half = "▀";
    const char *symbol = center;
    int offset_x = 0, offset_y = 0;
    if(in_data->aimingX < 8000)
        offset_x = -3;
    else if(in_data->aimingX < 16000)
        offset_x = -2;
    else if(in_data->aimingX < 24000)
        offset_x = -1;
    else if(in_data->aimingX < 40000)
        offset_x = 0;
    else if(in_data->aimingX < 48000)
        offset_x = 1;
    else if(in_data->aimingX < 56000)
        offset_x = 2;
    else
        offset_x = 3;

    if(in_data->aimingY < 8000) {
        offset_y = -1;
        symbol = upper_half;
    } else if(in_data->aimingY < 16000) {
        offset_y = -1;
        symbol = lower_half;
    } else if(in_data->aimingY < 24000) {
        offset_y = 0;
        symbol = upper_half;
    } else if(in_data->aimingY < 36000) {
        offset_y = 0;
        symbol = center;
    } else if(in_data->aimingY < 44000) {
        offset_y = 0;
        symbol = lower_half;
    } else if(in_data->aimingY < 56000) {
        offset_y = 1;
        symbol = upper_half;
    } else {
        offset_y = 1;
        symbol = lower_half;
    }

    mvaddstr(y + 2 + offset_y, x + 4 + offset_x, symbol);
    //mvaddstr(y + 2 + offset_y, x + 4 + offset_x, "⊕ ");
}

void draw_right_block(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(   y, x, "╭─────────────────────────────╮");
    mvaddstr( y+1, x, "│                   ──🯓       │");
    mvaddstr( y+2, x, "│                             │");
    mvaddstr( y+3, x, "│                             │");
    mvaddstr( y+4, x, "│                      ┌───┐  │");
    mvaddstr( y+5, x, "│                  🭺🭺🭺🭺🭺   ╎  │");
    mvaddstr( y+6, x, "│      ▁  ▁▁▁▁▁   ▁    │   │  │");
    mvaddstr( y+7, x, "│   ▕      🭺🭺🭺    🭺▏🭺🭺🭺🭺   ╎  │");
    mvaddstr( y+8, x, "│   ▕🭺🭺                │   │  │");
    mvaddstr( y+9, x, "│ ▁▁▁▁▁               🭺🭺   ╎  │");
    mvaddstr(y+10, x, "│   └───  ─────        └───┘  │");
    mvaddstr(y+11, x, "╰─────────────────────────────╯");

    mvaddstr(y-3, x+4, "🮣───🮢 🮣───🮢");
    mvaddstr(y-2, x+4, "│   │ 𜸗▁▁▁𜸙");
    mvaddstr(y-1, x+4, "🮡───🮠 🮡───🮠");

	attron(COLOR_PAIR(4));
    mvaddstr(y + 1, x + 23, "𜹸   𜹤");
    mvaddstr(y + 2, x + 23, "𜹺   𜹥");
    mvaddstr(y + 3, x + 23, "𜹒𜹓𜹓𜹓𜹑");
    attroff(COLOR_PAIR(4));
    attron(COLOR_PAIR(1));
    draw_big_button(x + 24, y + 2, in_data->dButtons.Eject);
    draw_big_button(x + 24, y + 9, in_data->dButtons.Start);
    mvaddstr(y-2, x+5, in_data->dButtons.LockOn ? "●" : "◍");
    mvaddstr(y-2, x+7, in_data->dButtons.MainWeapon ? "▌" : "🮌");
    mvaddstr(y-1, x+12, in_data->dButtons.Fire ? "█" : "▒"); //"𜱃");

    attroff(COLOR_PAIR(1));
    attron(COLOR_PAIR(color_variant));
    draw_wide_button(x + 7, y + 7, in_data->dButtons.MultiMonitorOpenClose);
    draw_wide_button(x + 14, y + 7, in_data->dButtons.MultiMonitorMapZoomInOut);
    draw_wide_button(x + 7, y + 8, in_data->dButtons.MultiMonitorModeSelect);
    draw_wide_button(x + 14, y + 8, in_data->dButtons.MultiMonitorSubMonitor);
    draw_wide_button(x + 7, y + 9, in_data->dButtons.MainMonitorZoomIn);
    draw_wide_button(x + 14, y + 9, in_data->dButtons.MainMonitorZoomOut);
    draw_big_button(x + 24, y + 5, in_data->dButtons.CockpitHatch);
    draw_big_button(x + 24, y + 7, in_data->dButtons.Ignition);
    attroff(COLOR_PAIR(color_variant));

    draw_aiming_lever(x+5, y+1, in_data);
}

void draw_pedal(int x, int y, int stem_length)
{
    mvaddstr(    y - stem_length, x, "╭───╮");
    mvaddstr(y + 1 - stem_length, x, "│○ ○│");
    mvaddstr(y + 2 - stem_length, x, "│○ ○│");
    mvaddstr(y + 3 - stem_length, x, "│○ ○│");
    mvaddstr(y + 4 - stem_length, x, "╰───╯");
	}

void draw_bent_pedal(int x, int y, int stem_length)
{
    mvaddstr(    y - stem_length, x,    "🯮🭻🭻🭻🯭");
    mvaddstr(y + 1 - stem_length, x,    "🯕 ○ 🯕");
    mvaddstr(y + 2 - stem_length, x + 1, "🯔 ○ 🯔");
    mvaddstr(y + 3 - stem_length, x + 1, "│ ○ │");
    mvaddstr(y + 4 - stem_length, x + 1, "╰───╯");
}

void draw_gas_pedal(int x, int y, int stem_length)
{
    mvaddstr(    y - stem_length, x, "╭───╮");
    mvaddstr(y + 1 - stem_length, x, "│ ○ │");
    mvaddstr(y + 2 - stem_length, x, "│ ○ │");
    mvaddstr(y + 3 - stem_length, x, "│ ○ │");
    mvaddstr(y + 4 - stem_length, x, "╰───╯");
}

void draw_pedals(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    mvaddstr(  y, x, "       🯐──────────────╮");
    mvaddstr(y+1, x, "╭─────🯑               │");
    mvaddstr(y+2, x, "│                     │");
    mvaddstr(y+3, x, "╞═════════════════════╡");
    mvaddstr(y+4, x, "├─────────────────────┤");
    mvaddstr(y+5, x, "│ 𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪 │");
    mvaddstr(y+6, x, "│ 𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪𜴪 │");
    mvaddstr(y+7, x, "├───🯑🭽▔▔▔▔▔▔🭾🯒────────┤");
    mvaddstr(y+8, x, "╰───🯑▔▔▔▔▔▔▔▔🯒────────╯");

    draw_bent_pedal(x +  1, y - 1, in_data->leftPedal   / 32768);
    draw_pedal(x +  9, y - 1, in_data->middlePedal / 32768);
    draw_gas_pedal(x + 16, y - 1, in_data->rightPedal  / 32768);
}


void draw_steel_battalion_controller(int x, int y, USB_SteelBattalion_InReport_t *in_data)
{
    draw_left_block(x, y + 1, in_data);
    draw_center_block(x+33, y, in_data);
    draw_right_block(x+61, y + 1, in_data);
    draw_pedals(x+34, y + 15, in_data);
    mvaddch(y+2, x+30, ACS_LTEE);
    mvaddch(y+2, x+31, ACS_HLINE);
    mvaddch(y+2, x+32, ACS_HLINE);
    mvaddch(y+2, x+33, ACS_RTEE);
    mvaddch(y+11, x+30, ACS_LTEE);
    mvaddch(y+11, x+31, ACS_HLINE);
    mvaddch(y+11, x+32, ACS_HLINE);
    mvaddch(y+11, x+33, ACS_RTEE);
    mvaddch(y+2, x+58, ACS_LTEE);
    mvaddch(y+2, x+59, ACS_HLINE);
    mvaddch(y+2, x+60, ACS_HLINE);
    mvaddch(y+2, x+61, ACS_RTEE);
    mvaddch(y+11, x+58, ACS_LTEE);
    mvaddch(y+11, x+59, ACS_HLINE);
    mvaddch(y+11, x+60, ACS_HLINE);
    mvaddch(y+11, x+61, ACS_RTEE);
}
