/*
 * uidrivestatus.c - GTK only, UI controls for Drive emulation
 *
 * Written by
 *  Ettore Perazzoli
 *  Oliver Schaertel
 *  pottendo <pottendo@gmx.net>
 *  groepaz <groepaz@gmx.net>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

/* #define DEBUG_X11UI */

#ifdef DEBUG_X11UI
#define DBG(_x_) log_debug _x_
#else
#define DBG(_x_)
#endif

#include "vice.h"

#include <string.h>

#include "drive/drive.h"
#include "lib.h"
#include "machine.h"
#include "ui.h"
#include "uiarch.h"
#include "uicontents.h"
#include "uifliplist.h"
#include "uimenu.h"
#include "util.h"
#include "video.h"
#include "resources.h"
#include "videoarch.h"

#define LED_WIDTH 12
#define LED_HEIGHT 6

char last_attached_images[NUM_DRIVES][256]; /* FIXME: We want this to be static.  */
char *last_menus[NUM_DRIVES];  /* FIXME: We want this to be static.  */
GtkWidget *last_drive_menus[NUM_DRIVES];  /* FIXME: We want this to be static.  */

/* Enabled drives.  */
ui_drive_enable_t enabled_drives = UI_DRIVE_ENABLE_NONE;  /* used also in uicommands */
/* Color of the drive active LED.  */
static int *drive_active_led;

static GtkWidget *drive_menus[NUM_DRIVES];

/*******************************************************************************
 * Floppy Drive related stuff
 ******************************************************************************/

void ui_destroy_drive_menu(int drvnr)
{
    if (drive_menus[drvnr]) {
        gtk_widget_destroy(drive_menus[drvnr]);
    }
}

void ui_set_drive_menu(int drvnr, ui_menu_entry_t *menu)
{
    drive_menus[drvnr] = gtk_menu_new();
    ui_menu_create(drive_menus[drvnr], NULL, "DriveMenu", menu);
}

static gboolean fliplist_popup_cb(GtkWidget *w, GdkEvent *event, gpointer data)
{
    int d = vice_ptr_to_int(data);
    if (event->type == GDK_BUTTON_PRESS) {
        GdkEventButton *bevent = (GdkEventButton*) event;
        if (bevent->button == 1) {
            if ((d >= 0)&& (d < NUM_DRIVES)) {
                uifliplist_update_menus(d + 8, d + 8);
                ui_menu_update_all_GTK();
                if (drive_menus[d]) {
                    gtk_menu_popup(GTK_MENU(drive_menus[d]), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
                }
            }
        } else if (bevent->button == 3) {
            if (strcmp(last_attached_images[d], "") == 0) {
                if (last_menus[d]) {
                    if (last_drive_menus[d]) {
                        gtk_widget_destroy(last_drive_menus[d]);
                    }
                    lib_free(last_menus[d]);
                    last_menus[d] = NULL;
                }
                return 0;
            }

            if (last_drive_menus[d]) {
                gtk_widget_destroy(last_drive_menus[d]);
            }
            lib_free(last_menus[d]);
            last_menus[d] = lib_stralloc(last_attached_images[d]);
            last_drive_menus[d] =
                rebuild_contents_menu(d+8, last_menus[d]);

            if (last_drive_menus[d]) {
                gtk_menu_popup(GTK_MENU(last_drive_menus[d]), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
            }
        }
    }
    return 0;
}

GtkWidget *build_drive_status_widget(app_shell_type *as, GdkWindow *window)
{
    GtkWidget *drive_box, *frame;
    int i;
    char *empty = util_concat("<", _("empty"), ">", NULL);

    drive_box = gtk_hbox_new(FALSE, 0);
    for (i = 0; i < NUM_DRIVES; i++) {
        char label[256];

        as->drive_status[i].event_box = gtk_event_box_new();

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

        sprintf(label, _("Drive %d: "), i + 8);
        as->drive_status[i].box = gtk_hbox_new(FALSE, 0);

        gtk_container_add(GTK_CONTAINER(frame), as->drive_status[i].box);
        gtk_widget_show(as->drive_status[i].box);

        gtk_container_add(GTK_CONTAINER(as->drive_status[i].event_box), frame);
        gtk_widget_show(frame);

#if 0
        drive_tooltips[i] = gtk_tooltips_new();
        gtk_tooltips_set_tip(GTK_TOOLTIPS(drive_tooltips[i]), as->drive_status[i].box->parent->parent, empty, NULL);
#else
        gtk_widget_set_tooltip_text(GTK_WIDGET(as->drive_status[i].box->parent->parent), empty);
#endif
        /* Label */
        as->drive_status[i].label = (void *)gtk_label_new(g_strdup(label));
        gtk_box_pack_start(GTK_BOX(as->drive_status[i].box), (GtkWidget *)as->drive_status[i].label, TRUE, TRUE, 0);
        gtk_widget_show((GtkWidget *)as->drive_status[i].label);
#if 0
        as->drive_status[i].image = (void *)gtk_label_new(empty);
        gtk_container_add(GTK_CONTAINER(event_box), as->drive_status[i].image);
        gtk_widget_show(as->drive_status[i].image);
#endif
        /* Track Label */
        as->drive_status[i].track_label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(as->drive_status[i].box), as->drive_status[i].track_label, FALSE, FALSE, 0);
        gtk_widget_show(as->drive_status[i].track_label);      

        /* Single Led */
        as->drive_status[i].led_pixmap = gdk_pixmap_new(window, LED_WIDTH, LED_HEIGHT, -1);
        as->drive_status[i].led = gtk_image_new_from_pixmap(as->drive_status[i].led_pixmap, NULL);
        gtk_widget_set_size_request(as->drive_status[i].led, LED_WIDTH, LED_HEIGHT);
        gtk_box_pack_start(GTK_BOX(as->drive_status[i].box), (GtkWidget *)as->drive_status[i].led, FALSE, FALSE, 4);
        gtk_widget_show(as->drive_status[i].led);

        /* Led1 for double Led drive */
        as->drive_status[i].led1_pixmap = gdk_pixmap_new(window, LED_WIDTH / 2, LED_HEIGHT, -1);
        as->drive_status[i].led1 = gtk_image_new_from_pixmap(as->drive_status[i].led1_pixmap, NULL);
        gtk_widget_set_size_request(as->drive_status[i].led1, LED_WIDTH / 2, LED_HEIGHT);
        gtk_box_pack_start(GTK_BOX(as->drive_status[i].box), (GtkWidget *)as->drive_status[i].led1, FALSE, FALSE, 1);
        gtk_widget_show(as->drive_status[i].led1);

        /* Led2 for double Led drive */
        as->drive_status[i].led2_pixmap = gdk_pixmap_new(window, LED_WIDTH / 2, LED_HEIGHT, -1);
        as->drive_status[i].led2 = gtk_image_new_from_pixmap(as->drive_status[i].led2_pixmap, NULL);
        gtk_widget_set_size_request(as->drive_status[i].led2, LED_WIDTH / 2, LED_HEIGHT);
        gtk_box_pack_start(GTK_BOX(as->drive_status[i].box), (GtkWidget *)as->drive_status[i].led2, FALSE, FALSE, 1);
        gtk_widget_show(as->drive_status[i].led2);

        /* Pack everything together */
        gtk_box_pack_start(GTK_BOX(drive_box), as->drive_status[i].event_box, FALSE, FALSE, 0);

        gtk_widget_set_events(as->drive_status[i].event_box, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK);
        g_signal_connect(G_OBJECT(as->drive_status[i].event_box), "button-press-event", G_CALLBACK(fliplist_popup_cb), (gpointer)(int_to_void_ptr(i)));
        gtk_widget_show(as->drive_status[i].event_box);
    }
    lib_free(empty);
    return drive_box;
}

void ui_enable_drive_status(ui_drive_enable_t enable, int *drive_led_color)
{
    int i, j;
    int num_app_shells = get_num_shells();

    enabled_drives = enable;
    drive_active_led = drive_led_color;

    for (i = 0; i < num_app_shells; i++) {
        for (j = 0; j < NUM_DRIVES; j++) {
            if (enabled_drives && (enabled_drives & (1 << j))) {
                /* enabled + active drive */
                gtk_widget_show(app_shells[i].drive_status[j].event_box);
                gtk_widget_show(app_shells[i].drive_status[j].track_label);
                if (drive_num_leds(j) == 1) {
                    gtk_widget_show(app_shells[i].drive_status[j].led);
                    gtk_widget_hide(app_shells[i].drive_status[j].led1);
                    gtk_widget_hide(app_shells[i].drive_status[j].led2);
                } else {
                    gtk_widget_hide(app_shells[i].drive_status[j].led);
                    gtk_widget_show(app_shells[i].drive_status[j].led1);
                    gtk_widget_show(app_shells[i].drive_status[j].led2);
                }
            } else if (!enabled_drives && (strcmp(last_attached_images[j], "") != 0)) {
                gtk_widget_show(app_shells[i].drive_status[j].event_box);
                gtk_widget_hide(app_shells[i].drive_status[j].track_label);
                gtk_widget_hide(app_shells[i].drive_status[j].led);
                gtk_widget_hide(app_shells[i].drive_status[j].led1);
                gtk_widget_hide(app_shells[i].drive_status[j].led2);
            } else {
                gtk_widget_hide(app_shells[i].drive_status[j].event_box);
            }
        }
#if 0
        /* enable this when a dynamic multiline status bar is needed.
           be aware that x128 has weird sized windows on startup, because of 
           the uninitialized canvas window(-size) during startup */
        /* resize according to new needs */
        {
            int width, height;

            gdk_window_get_size(app_shells[i].canvas->window, &width, &height);
            ui_resize_canvas_window(app_shells[i].canvas, width, height);
        }
#endif
    }
}

void ui_init_drive_status_widget(void)
{
    int i;
    /* This is necessary because the status might have been set before we
    actually open the canvas window. e.g. by commandline */
    ui_enable_drive_status(enabled_drives, drive_active_led);

    /* make sure that all drive status widgets are initialized.
    This is needed for proper dual disk/dual led drives (8050, 8250). */
    for (i = 0; i < NUM_DRIVES; i++) {
        ui_display_drive_led(i, 1000, 1000);
    }
}

void ui_display_drive_track(unsigned int drive_number, unsigned int drive_base, unsigned int half_track_number)
{
    int i;
    static char str[256];
    double track_number = (double)half_track_number / 2.0;
    int num_app_shells = get_num_shells();

    sprintf(str, "%.1f", (double)track_number);

    for (i = 0; i < num_app_shells; i++) {
        gtk_label_set_text(GTK_LABEL(app_shells[i].drive_status[drive_number].track_label), str);
    }
}

void ui_display_drive_led(int drive_number, unsigned int led_pwm1, unsigned int led_pwm2)
{
    int status = 0;
    int i, ci1, ci2;
    GdkGC *app_gc = get_toplevel();
    int num_app_shells = get_num_shells();

    GdkColor *color;

    if (led_pwm1 > 100) {
        status |= 1;
    }
    if (led_pwm2 > 100) {
        status |= 2;
    }

    ci1 = (int)(((float)led_pwm1 / MAX_PWM) * 16) - 1;
    ci2 = (int)(((float)led_pwm2 / MAX_PWM) * 16) - 1;
    if (ci1 < 0) {
        ci1 = 0;
    }
    if (ci2 < 0) {
        ci2 = 0;
    }

    for (i = 0; i < num_app_shells; i++) {
        drive_status_widget *ds = &app_shells[i].drive_status[drive_number];

        color = status ? (drive_active_led[drive_number] ? &drive_led_on_green_pixels[ci1] : &drive_led_on_red_pixels[ci1]) : &drive_led_off_pixel;
        gdk_gc_set_rgb_fg_color(app_gc, color);
        gdk_draw_rectangle(ds->led_pixmap, app_gc, TRUE, 0, 0, LED_WIDTH, LED_HEIGHT);
        gtk_widget_queue_draw(ds->led);

        color = (status & 1) ? (drive_active_led[drive_number] ? &drive_led_on_green_pixels[ci1] : &drive_led_on_red_pixels[ci1]) : &drive_led_off_pixel;
        gdk_gc_set_rgb_fg_color(app_gc, color);
        gdk_draw_rectangle(ds->led1_pixmap, app_gc, TRUE, 0, 0, LED_WIDTH / 2, LED_HEIGHT);
        gtk_widget_queue_draw(ds->led1);

        color = (status & 2) ? (drive_active_led[drive_number] ? &drive_led_on_green_pixels[ci2] : &drive_led_on_red_pixels[ci2]) : &drive_led_off_pixel;
        gdk_gc_set_rgb_fg_color(app_gc, color);
        gdk_draw_rectangle(ds->led2_pixmap, app_gc, TRUE, 0, 0, LED_WIDTH / 2, LED_HEIGHT);
        gtk_widget_queue_draw(ds->led2);
    }
}

void ui_display_drive_current_image(unsigned int drive_number, const char *image)
{
    int i;
    char *name;
    int num_app_shells = get_num_shells();

    if (console_mode) {
        return;
    }

    /* FIXME: Allow more than two drives.  */
    if (drive_number >= NUM_DRIVES) {
        return;
    }

    strcpy(&(last_attached_images[drive_number][0]), image);
    util_fname_split(&(last_attached_images[drive_number][0]), NULL, &name);

    if (strcmp(name, "") == 0) {
        lib_free(name);
        name = util_concat("<", _("empty"), ">", NULL);
    }

    for (i = 0; i < num_app_shells; i++) {
#if 0
        gtk_label_set_text(GTK_LABEL(app_shells[i].drive_status[drive_number].image), name);
#endif
        gtk_widget_set_tooltip_text(GTK_WIDGET(app_shells[i].drive_status[drive_number].box->parent->parent), name);
    }
    lib_free(name);

    ui_enable_drive_status(enabled_drives, drive_active_led);
}
 