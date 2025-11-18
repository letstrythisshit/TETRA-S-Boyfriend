/*
 * GTK+ GUI Interface for TETRA Analyzer
 * Provides real-time control of detection parameters
 */

#include "tetra_analyzer.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GUI context structure
struct tetra_gui_t {
    // GTK widgets
    GtkWidget *window;
    GtkWidget *freq_spin;
    GtkWidget *min_power_scale;
    GtkWidget *strong_match_scale;
    GtkWidget *moderate_match_scale;
    GtkWidget *strong_corr_scale;
    GtkWidget *moderate_corr_scale;
    GtkWidget *lpf_cutoff_scale;
    GtkWidget *power_mult_scale;

    // Status display widgets
    GtkWidget *signal_power_label;
    GtkWidget *match_count_label;
    GtkWidget *correlation_label;
    GtkWidget *detection_count_label;
    GtkWidget *status_indicator;

    // Parameter value labels
    GtkWidget *min_power_value;
    GtkWidget *strong_match_value;
    GtkWidget *moderate_match_value;
    GtkWidget *strong_corr_value;
    GtkWidget *moderate_corr_value;
    GtkWidget *lpf_cutoff_value;
    GtkWidget *power_mult_value;

    // Shared state
    tetra_config_t *config;
    detection_params_t *params;
    detection_status_t *status;
    rtl_sdr_t *sdr;

    // Update timer
    guint update_timer_id;
    bool running;
};

// Forward declarations
static gboolean update_status_display(gpointer user_data);
static void on_freq_changed(GtkSpinButton *spin, gpointer user_data);
static void on_parameter_changed(GtkRange *range, gpointer user_data);
static void on_reset_defaults(GtkButton *button, gpointer user_data);
static void on_window_destroy(GtkWidget *widget, gpointer user_data);

// Create a labeled scale widget with value display
static GtkWidget* create_labeled_scale(const char *label, double min, double max,
                                       double step, double value,
                                       GtkWidget **scale_out, GtkWidget **value_label_out,
                                       GCallback callback, gpointer user_data) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 5);
    gtk_widget_set_margin_end(box, 5);

    // Label
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 200, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    // Scale
    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_LEFT);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_range_set_value(GTK_RANGE(scale), value);
    gtk_widget_set_size_request(scale, 300, -1);
    gtk_box_pack_start(GTK_BOX(box), scale, TRUE, TRUE, 0);

    if (callback) {
        g_signal_connect(scale, "value-changed", callback, user_data);
    }

    // Value label
    GtkWidget *val_label = gtk_label_new("");
    gtk_widget_set_size_request(val_label, 80, -1);
    gtk_label_set_xalign(GTK_LABEL(val_label), 1.0);
    gtk_box_pack_start(GTK_BOX(box), val_label, FALSE, FALSE, 0);

    if (scale_out) *scale_out = scale;
    if (value_label_out) *value_label_out = val_label;

    return box;
}

// Create a status display row
static GtkWidget* create_status_row(const char *label, GtkWidget **value_label_out) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box, 5);
    gtk_widget_set_margin_end(box, 5);

    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_size_request(lbl, 150, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    GtkWidget *val_label = gtk_label_new("--");
    gtk_label_set_xalign(GTK_LABEL(val_label), 0.0);
    gtk_box_pack_start(GTK_BOX(box), val_label, TRUE, TRUE, 0);

    if (value_label_out) *value_label_out = val_label;

    return box;
}

// Update parameter value labels
static void update_parameter_labels(tetra_gui_t *gui) {
    char buf[64];

    pthread_mutex_lock(&gui->params->lock);

    snprintf(buf, sizeof(buf), "%.1f", gui->params->min_signal_power);
    gtk_label_set_text(GTK_LABEL(gui->min_power_value), buf);

    snprintf(buf, sizeof(buf), "%d/22", gui->params->strong_match_threshold);
    gtk_label_set_text(GTK_LABEL(gui->strong_match_value), buf);

    snprintf(buf, sizeof(buf), "%d/22", gui->params->moderate_match_threshold);
    gtk_label_set_text(GTK_LABEL(gui->moderate_match_value), buf);

    snprintf(buf, sizeof(buf), "%.2f", gui->params->strong_correlation);
    gtk_label_set_text(GTK_LABEL(gui->strong_corr_value), buf);

    snprintf(buf, sizeof(buf), "%.2f", gui->params->moderate_correlation);
    gtk_label_set_text(GTK_LABEL(gui->moderate_corr_value), buf);

    snprintf(buf, sizeof(buf), "%.2f", gui->params->lpf_cutoff);
    gtk_label_set_text(GTK_LABEL(gui->lpf_cutoff_value), buf);

    snprintf(buf, sizeof(buf), "%.2f", gui->params->moderate_power_multiplier);
    gtk_label_set_text(GTK_LABEL(gui->power_mult_value), buf);

    pthread_mutex_unlock(&gui->params->lock);
}

// Callback for frequency changes
static void on_freq_changed(GtkSpinButton *spin, gpointer user_data) {
    tetra_gui_t *gui = (tetra_gui_t*)user_data;
    uint32_t new_freq = (uint32_t)gtk_spin_button_get_value(spin);

    gui->config->frequency = new_freq;

    // Update SDR frequency if running
    if (gui->sdr && gui->sdr->running) {
        // Note: Would need to implement rtl_sdr_set_frequency() in rtl_interface.c
        log_message(gui->config->verbose, "Frequency changed to %u Hz\n", new_freq);
    }
}

// Callback for parameter changes
static void on_parameter_changed(GtkRange *range, gpointer user_data) {
    tetra_gui_t *gui = (tetra_gui_t*)user_data;

    pthread_mutex_lock(&gui->params->lock);

    if (GTK_WIDGET(range) == gui->min_power_scale) {
        gui->params->min_signal_power = gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->strong_match_scale) {
        gui->params->strong_match_threshold = (int)gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->moderate_match_scale) {
        gui->params->moderate_match_threshold = (int)gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->strong_corr_scale) {
        gui->params->strong_correlation = gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->moderate_corr_scale) {
        gui->params->moderate_correlation = gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->lpf_cutoff_scale) {
        gui->params->lpf_cutoff = gtk_range_get_value(range);
    } else if (GTK_WIDGET(range) == gui->power_mult_scale) {
        gui->params->moderate_power_multiplier = gtk_range_get_value(range);
    }

    pthread_mutex_unlock(&gui->params->lock);

    update_parameter_labels(gui);
}

// Callback for reset defaults button
static void on_reset_defaults(GtkButton *button, gpointer user_data) {
    tetra_gui_t *gui = (tetra_gui_t*)user_data;

    detection_params_reset_defaults(gui->params);

    // Update all scale widgets
    pthread_mutex_lock(&gui->params->lock);
    gtk_range_set_value(GTK_RANGE(gui->min_power_scale), gui->params->min_signal_power);
    gtk_range_set_value(GTK_RANGE(gui->strong_match_scale), gui->params->strong_match_threshold);
    gtk_range_set_value(GTK_RANGE(gui->moderate_match_scale), gui->params->moderate_match_threshold);
    gtk_range_set_value(GTK_RANGE(gui->strong_corr_scale), gui->params->strong_correlation);
    gtk_range_set_value(GTK_RANGE(gui->moderate_corr_scale), gui->params->moderate_correlation);
    gtk_range_set_value(GTK_RANGE(gui->lpf_cutoff_scale), gui->params->lpf_cutoff);
    gtk_range_set_value(GTK_RANGE(gui->power_mult_scale), gui->params->moderate_power_multiplier);
    pthread_mutex_unlock(&gui->params->lock);

    update_parameter_labels(gui);
}

// Update status display (called by timer)
static gboolean update_status_display(gpointer user_data) {
    tetra_gui_t *gui = (tetra_gui_t*)user_data;
    char buf[128];

    pthread_mutex_lock(&gui->status->lock);

    // Update signal power
    snprintf(buf, sizeof(buf), "%.2f", gui->status->current_signal_power);
    gtk_label_set_text(GTK_LABEL(gui->signal_power_label), buf);

    // Update match count
    snprintf(buf, sizeof(buf), "%d/22", gui->status->last_match_count);
    gtk_label_set_text(GTK_LABEL(gui->match_count_label), buf);

    // Update correlation
    snprintf(buf, sizeof(buf), "%.3f", gui->status->last_correlation);
    gtk_label_set_text(GTK_LABEL(gui->correlation_label), buf);

    // Update detection count
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)gui->status->detection_count);
    gtk_label_set_text(GTK_LABEL(gui->detection_count_label), buf);

    // Update status indicator color
    if (gui->status->burst_detected) {
        // Green for detected
        gtk_widget_set_name(gui->status_indicator, "status-detected");
    } else if (gui->status->current_signal_power > 1.0f) {
        // Yellow for signal present but not detected
        gtk_widget_set_name(gui->status_indicator, "status-signal");
    } else {
        // Red for no signal
        gtk_widget_set_name(gui->status_indicator, "status-idle");
    }

    pthread_mutex_unlock(&gui->status->lock);

    return G_SOURCE_CONTINUE;
}

// Window destroy callback
static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    gtk_main_quit();
}

// Initialize GUI
tetra_gui_t* tetra_gui_init(tetra_config_t *config, detection_params_t *params,
                            detection_status_t *status, rtl_sdr_t *sdr) {
    tetra_gui_t *gui = calloc(1, sizeof(tetra_gui_t));
    if (!gui) {
        fprintf(stderr, "Failed to allocate GUI context\n");
        return NULL;
    }

    gui->config = config;
    gui->params = params;
    gui->status = status;
    gui->sdr = sdr;
    gui->running = true;

    // Initialize GTK
    gtk_init(NULL, NULL);

    // Create main window
    gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui->window), "TETRA Analyzer Control Panel");
    gtk_window_set_default_size(GTK_WINDOW(gui->window), 700, 600);
    gtk_container_set_border_width(GTK_CONTAINER(gui->window), 10);
    g_signal_connect(gui->window, "destroy", G_CALLBACK(on_window_destroy), gui);

    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(gui->window), main_box);

    // Title
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b><big>TETRA Analyzer Control Panel</big></b>");
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 5);

    // Separator
    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    // === FREQUENCY CONTROL ===
    GtkWidget *freq_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(freq_box, 5);
    gtk_widget_set_margin_end(freq_box, 5);

    GtkWidget *freq_label = gtk_label_new("Frequency (Hz):");
    gtk_widget_set_size_request(freq_label, 200, -1);
    gtk_label_set_xalign(GTK_LABEL(freq_label), 0.0);
    gtk_box_pack_start(GTK_BOX(freq_box), freq_label, FALSE, FALSE, 0);

    gui->freq_spin = gtk_spin_button_new_with_range(TETRA_FREQUENCY_MIN, TETRA_FREQUENCY_MAX, 1000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(gui->freq_spin), config->frequency);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(gui->freq_spin), 0);
    gtk_widget_set_size_request(gui->freq_spin, 200, -1);
    g_signal_connect(gui->freq_spin, "value-changed", G_CALLBACK(on_freq_changed), gui);
    gtk_box_pack_start(GTK_BOX(freq_box), gui->freq_spin, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), freq_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // === DETECTION PARAMETERS ===
    GtkWidget *params_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(params_label), "<b>Detection Parameters</b>");
    gtk_label_set_xalign(GTK_LABEL(params_label), 0.0);
    gtk_box_pack_start(GTK_BOX(main_box), params_label, FALSE, FALSE, 0);

    // Min Signal Power
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Min Signal Power:", 1.0, 20.0, 0.5, params->min_signal_power,
                           &gui->min_power_scale, &gui->min_power_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Strong Match Threshold
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Strong Match Threshold:", 18, 22, 1, params->strong_match_threshold,
                           &gui->strong_match_scale, &gui->strong_match_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Moderate Match Threshold
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Moderate Match Threshold:", 15, 22, 1, params->moderate_match_threshold,
                           &gui->moderate_match_scale, &gui->moderate_match_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Strong Correlation
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Strong Correlation:", 0.5, 1.0, 0.05, params->strong_correlation,
                           &gui->strong_corr_scale, &gui->strong_corr_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Moderate Correlation
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Moderate Correlation:", 0.5, 1.0, 0.05, params->moderate_correlation,
                           &gui->moderate_corr_scale, &gui->moderate_corr_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Low-Pass Filter Cutoff
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Low-Pass Filter Cutoff:", 0.1, 1.0, 0.05, params->lpf_cutoff,
                           &gui->lpf_cutoff_scale, &gui->lpf_cutoff_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Power Multiplier
    gtk_box_pack_start(GTK_BOX(main_box),
        create_labeled_scale("Moderate Power Multiplier:", 1.0, 2.0, 0.1, params->moderate_power_multiplier,
                           &gui->power_mult_scale, &gui->power_mult_value,
                           G_CALLBACK(on_parameter_changed), gui),
        FALSE, FALSE, 0);

    // Reset button
    GtkWidget *reset_button = gtk_button_new_with_label("Reset to Defaults");
    g_signal_connect(reset_button, "clicked", G_CALLBACK(on_reset_defaults), gui);
    gtk_box_pack_start(GTK_BOX(main_box), reset_button, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(main_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // === STATUS DISPLAY ===
    GtkWidget *status_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(status_label), "<b>Real-time Status</b>");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(main_box), status_label, FALSE, FALSE, 0);

    // Status indicator
    GtkWidget *indicator_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(indicator_box, 5);
    gtk_widget_set_margin_end(indicator_box, 5);

    GtkWidget *indicator_label = gtk_label_new("Detection Status:");
    gtk_widget_set_size_request(indicator_label, 150, -1);
    gtk_label_set_xalign(GTK_LABEL(indicator_label), 0.0);
    gtk_box_pack_start(GTK_BOX(indicator_box), indicator_label, FALSE, FALSE, 0);

    gui->status_indicator = gtk_drawing_area_new();
    gtk_widget_set_size_request(gui->status_indicator, 20, 20);
    gtk_widget_set_name(gui->status_indicator, "status-idle");
    gtk_box_pack_start(GTK_BOX(indicator_box), gui->status_indicator, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box), indicator_box, FALSE, FALSE, 0);

    // Status rows
    gtk_box_pack_start(GTK_BOX(main_box),
        create_status_row("Signal Power:", &gui->signal_power_label),
        FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box),
        create_status_row("Last Match Count:", &gui->match_count_label),
        FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box),
        create_status_row("Last Correlation:", &gui->correlation_label),
        FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_box),
        create_status_row("Total Detections:", &gui->detection_count_label),
        FALSE, FALSE, 0);

    // CSS for status indicator colors
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data =
        "#status-detected { background-color: #00ff00; border-radius: 10px; }\n"
        "#status-signal { background-color: #ffff00; border-radius: 10px; }\n"
        "#status-idle { background-color: #ff0000; border-radius: 10px; }\n";
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Initialize parameter labels
    update_parameter_labels(gui);

    // Show all widgets
    gtk_widget_show_all(gui->window);

    // Start update timer (100ms interval)
    gui->update_timer_id = g_timeout_add(100, update_status_display, gui);

    return gui;
}

// Run GUI main loop
void tetra_gui_run(tetra_gui_t *gui) {
    if (!gui) return;
    gtk_main();
}

// Update status (can be called from external thread)
void tetra_gui_update_status(tetra_gui_t *gui) {
    // Status is automatically updated by timer
    // This function is here for API compatibility
    (void)gui;
}

// Cleanup GUI
void tetra_gui_cleanup(tetra_gui_t *gui) {
    if (!gui) return;

    if (gui->update_timer_id > 0) {
        g_source_remove(gui->update_timer_id);
    }

    gui->running = false;
    free(gui);
}
