#include <Wire.h>  // Commuication I2C
#include <lvgl.h>  // Gestion d'affichage 
#include <demos/lv_demos.h>  // exemple 
#include <examples/lv_examples.h> // exemple 
#include <PCA9557.h>  // Driver circuit Tactil
#include "gfx_config.h"  // fichier de Parametrage 

extern const lv_img_dsc_t stellantis_logo;  // Declaration

PCA9557 Out;

/* =====================================================
   BUFFER LVGL
   ===================================================== */

static lv_disp_draw_buf_t draw_buf;

static lv_color_t disp_draw_buf1[
    screenWidth * screenHeight / 8
];

static lv_color_t disp_draw_buf2[
    screenWidth * screenHeight / 8
];


/* =====================================================
   AFFICHAGE LVGL -> ECRAN
   ===================================================== */

void my_disp_flush(
    lv_disp_drv_t *disp,
    const lv_area_t *area,
    lv_color_t *color_p
)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();

    tft.setAddrWindow(
        area->x1,
        area->y1,
        w,
        h
    );

    tft.writePixels(
        (lgfx::rgb565_t *)&color_p->full,
        w * h
    );

    tft.endWrite();

    lv_disp_flush_ready(disp);
}


/* =====================================================
   LECTURE DU TACTILE
   ===================================================== */

void my_touchpad_read(
    lv_indev_drv_t *indev_driver,
    lv_indev_data_t *data
)
{
    uint16_t touchX;
    uint16_t touchY;

    bool touched = tft.getTouch(
        &touchX,
        &touchY
    );

    if (touched)
    {
        data->state = LV_INDEV_STATE_PR;

        data->point.x = touchX;
        data->point.y = touchY;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}


/* =====================================================
   INITIALISATION LVGL
   ===================================================== */

void lvgl_driver_function_init()
{
    lv_disp_draw_buf_init(
        &draw_buf,
        disp_draw_buf1,
        disp_draw_buf2,
        screenWidth * screenHeight / 8
    );

    /* -----------------------------
       DISPLAY
       ----------------------------- */

    static lv_disp_drv_t disp_drv;

    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;

    disp_drv.flush_cb = my_disp_flush;

    disp_drv.full_refresh = 1;

    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);


    /* -----------------------------
       TOUCH
       ----------------------------- */

    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_POINTER;

    indev_drv.read_cb = my_touchpad_read;

    lv_indev_drv_register(&indev_drv);
}


/* =====================================================
   OBJETS DE L'INTERFACE
   ===================================================== */

lv_obj_t *screen_parametres;

lv_obj_t *ta_epaisseur;
lv_obj_t *ta_longueur;
lv_obj_t *ta_position;
lv_obj_t *ta_quantite;

lv_obj_t *keyboard;


// =====================================================
// OBJETS DE L'ECRAN 3 : CYCLE
// =====================================================

lv_obj_t *screen_cycle;

lv_obj_t *label_etat;
lv_obj_t *label_piece;
lv_obj_t *label_x_cycle;
lv_obj_t *label_y_cycle;

lv_obj_t *bar_progression;    

lv_obj_t *btn_pause;
lv_obj_t *btn_stop;

lv_obj_t *label_pause;
lv_obj_t *label_stop;


/* =====================================================
   FONCTION POUR AFFICHER LE CLAVIER
   ===================================================== */

void show_keyboard(lv_obj_t *textarea)
{
    if (keyboard == NULL)
        return;

    lv_keyboard_set_textarea(
        keyboard,
        textarea
    );

    lv_obj_clear_flag(
        keyboard,
        LV_OBJ_FLAG_HIDDEN
    );
}


/* =====================================================
   FONCTION POUR CACHER LE CLAVIER
   ===================================================== */

void hide_keyboard()
{
    if (keyboard == NULL)
        return;

    lv_obj_add_flag(
        keyboard,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_keyboard_set_textarea(
        keyboard,
        NULL
    );
}


/* =====================================================
   EVENEMENT DES CHAMPS TEXTE
   ===================================================== */

static void textarea_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED)
    {
        lv_obj_t *textarea =
            lv_event_get_target(e);

        show_keyboard(textarea);
    }
}


// =====================================================
// CREATION DE L'ECRAN 2 : PARAMETRES DE COUPE
// STYLE INDUSTRIEL GRIS / BLEU
// =====================================================

void create_screen_parametres()
{
    // =================================================
    // CREATION DE L'ECRAN
    // =================================================

    screen_parametres = lv_obj_create(NULL);

    // Fond gris clair industriel
    lv_obj_set_style_bg_color(
        screen_parametres,
        lv_color_hex(0xE8ECF1),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        screen_parametres,
        0,
        LV_PART_MAIN
    );


    // =================================================
    // EN-TETE BLEU
    // =================================================

    lv_obj_t *header =
        lv_obj_create(screen_parametres);

    lv_obj_set_size(
        header,
        800,
        60
    );

    lv_obj_align(
        header,
        LV_ALIGN_TOP_MID,
        0,
        0
    );

    lv_obj_set_style_bg_color(
        header,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        header,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        header,
        0,
        LV_PART_MAIN
    );


    // =================================================
    // TITRE
    // =================================================

    lv_obj_t *title =
        lv_label_create(header);

    lv_label_set_text(
        title,
        "PARAMETRES DE COUPE"
    );

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        title,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_center(title);


    // =================================================
    // STYLE COMMUN DES LABELS
    // =================================================

    // -------------------------------------------------
    // EPAISSEUR
    // -------------------------------------------------

    lv_obj_t *label_epaisseur =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        label_epaisseur,
        "Epaisseur"
    );

    lv_obj_set_style_text_font(
        label_epaisseur,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_epaisseur,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_epaisseur,
        LV_ALIGN_TOP_LEFT,
        70,
        85
    );


    // =================================================
    // CHAMP EPAISSEUR
    // =================================================

    ta_epaisseur =
        lv_textarea_create(screen_parametres);

    lv_textarea_set_text(
        ta_epaisseur,
        "10.0"
    );

    lv_textarea_set_one_line(
        ta_epaisseur,
        true
    );

    lv_textarea_set_cursor_click_pos(
        ta_epaisseur,
        true
    );

    lv_obj_set_size(
        ta_epaisseur,
        160,
        42
    );

    lv_obj_align(
        ta_epaisseur,
        LV_ALIGN_TOP_LEFT,
        230,
        76
    );

    lv_obj_set_style_bg_color(
        ta_epaisseur,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        ta_epaisseur,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        ta_epaisseur,
        2,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        ta_epaisseur,
        6,
        LV_PART_MAIN
    );

    lv_obj_add_event_cb(
        ta_epaisseur,
        textarea_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );


    // Unité mm

    lv_obj_t *unit_epaisseur =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        unit_epaisseur,
        "mm"
    );

    lv_obj_set_style_text_font(
        unit_epaisseur,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        unit_epaisseur,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        unit_epaisseur,
        LV_ALIGN_TOP_LEFT,
        405,
        88
    );


    // =================================================
    // LONGUEUR X
    // =================================================

    lv_obj_t *label_x =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        label_x,
        "Longueur X"
    );

    lv_obj_set_style_text_font(
        label_x,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_x,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_x,
        LV_ALIGN_TOP_LEFT,
        70,
        140
    );


    ta_longueur =
        lv_textarea_create(screen_parametres);

    lv_textarea_set_text(
        ta_longueur,
        "500"
    );

    lv_textarea_set_one_line(
        ta_longueur,
        true
    );

    lv_obj_set_size(
        ta_longueur,
        160,
        42
    );

    lv_obj_align(
        ta_longueur,
        LV_ALIGN_TOP_LEFT,
        230,
        131
    );

    lv_obj_set_style_bg_color(
        ta_longueur,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        ta_longueur,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        ta_longueur,
        2,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        ta_longueur,
        6,
        LV_PART_MAIN
    );

    lv_obj_add_event_cb(
        ta_longueur,
        textarea_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );


    lv_obj_t *unit_x =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        unit_x,
        "mm"
    );

    lv_obj_set_style_text_font(
        unit_x,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        unit_x,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        unit_x,
        LV_ALIGN_TOP_LEFT,
        405,
        143
    );


    // =================================================
    // POSITION Y
    // =================================================

    lv_obj_t *label_y =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        label_y,
        "Position Y"
    );

    lv_obj_set_style_text_font(
        label_y,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_y,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_y,
        LV_ALIGN_TOP_LEFT,
        70,
        195
    );


    ta_position =
        lv_textarea_create(screen_parametres);

    lv_textarea_set_text(
        ta_position,
        "200"
    );

    lv_textarea_set_one_line(
        ta_position,
        true
    );

    lv_obj_set_size(
        ta_position,
        160,
        42
    );

    lv_obj_align(
        ta_position,
        LV_ALIGN_TOP_LEFT,
        230,
        186
    );

    lv_obj_set_style_bg_color(
        ta_position,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        ta_position,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        ta_position,
        2,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        ta_position,
        6,
        LV_PART_MAIN
    );

    lv_obj_add_event_cb(
        ta_position,
        textarea_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );


    lv_obj_t *unit_y =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        unit_y,
        "mm"
    );

    lv_obj_set_style_text_font(
        unit_y,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        unit_y,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        unit_y,
        LV_ALIGN_TOP_LEFT,
        405,
        198
    );


    // =================================================
    // QUANTITE
    // =================================================

    lv_obj_t *label_quantite =
        lv_label_create(screen_parametres);

    lv_label_set_text(
        label_quantite,
        "Quantite"
    );

    lv_obj_set_style_text_font(
        label_quantite,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_quantite,
        lv_color_hex(0x263238),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_quantite,
        LV_ALIGN_TOP_LEFT,
        70,
        250
    );


    ta_quantite =
        lv_textarea_create(screen_parametres);

    lv_textarea_set_text(
        ta_quantite,
        "5"
    );

    lv_textarea_set_one_line(
        ta_quantite,
        true
    );

    lv_obj_set_size(
        ta_quantite,
        160,
        42
    );

    lv_obj_align(
        ta_quantite,
        LV_ALIGN_TOP_LEFT,
        230,
        241
    );

    lv_obj_set_style_bg_color(
        ta_quantite,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        ta_quantite,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        ta_quantite,
        2,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        ta_quantite,
        6,
        LV_PART_MAIN
    );

    lv_obj_add_event_cb(
        ta_quantite,
        textarea_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );


    // =================================================
    // BOUTON DEMARRER
    // =================================================

    lv_obj_t *btn_start =
        lv_btn_create(screen_parametres);

    lv_obj_set_size(
        btn_start,
        190,
        55
    );

    lv_obj_align(
        btn_start,
        LV_ALIGN_BOTTOM_MID,
        0,
        -20
    );

    lv_obj_set_style_bg_color(
        btn_start,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        btn_start,
        8,
        LV_PART_MAIN
    );


    lv_obj_t *label_start =
        lv_label_create(btn_start);

    lv_label_set_text(
        label_start,
        "DEMARRER"
    );

    lv_obj_set_style_text_font(
        label_start,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_start,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_center(label_start);
    // =================================================
// ACTION DU BOUTON DEMARRER
// =================================================

    lv_obj_add_event_cb(
       btn_start,
       [](lv_event_t *e)
       {
        // Passage à l'écran 3
            create_screen_cycle();
       },
       LV_EVENT_CLICKED,
       NULL
    );


    // =================================================
    // CLAVIER NUMERIQUE
    // =================================================

    keyboard =
        lv_keyboard_create(screen_parametres);

    lv_keyboard_set_mode(
        keyboard,
        LV_KEYBOARD_MODE_NUMBER
    );

    lv_obj_set_size(
        keyboard,
        350,
        210
    );

    lv_obj_align(
        keyboard,
        LV_ALIGN_BOTTOM_RIGHT,
        0,
        0
    );


    // =================================================
    // STYLE DU CLAVIER
    // =================================================

    lv_obj_set_style_bg_color(
        keyboard,
        lv_color_hex(0xCFD8E3),
        LV_PART_MAIN
    );


    // =================================================
    // CLAVIER CACHE AU DEMARRAGE
    // =================================================

    lv_obj_add_flag(
        keyboard,
        LV_OBJ_FLAG_HIDDEN
    );


    // =================================================
    // CHARGER L'ECRAN
    // =================================================

    lv_scr_load(screen_parametres);
}
// =====================================================
// CREATION DE L'ECRAN 3 : CYCLE EN COURS
// =====================================================

void create_screen_cycle()
{
    // Création de l'écran
    screen_cycle = lv_obj_create(NULL);

    // =================================================
    // FOND GRIS INDUSTRIEL
    // =================================================

    lv_obj_set_style_bg_color(
        screen_cycle,
        lv_color_hex(0xE8E8E8),
        LV_PART_MAIN
    );

    // =================================================
    // TITRE
    // =================================================

    lv_obj_t *title = lv_label_create(screen_cycle);

    lv_label_set_text(
        title,
        "CYCLE EN COURS"
    );

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        title,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        15
    );

    // =================================================
    // ETAT
    // =================================================

    label_etat = lv_label_create(screen_cycle);

    lv_label_set_text(
        label_etat,
        "ETAT : COUPE EN COURS"
    );

    lv_obj_set_style_text_font(
        label_etat,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_etat,
        lv_color_hex(0x1565C0),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_etat,
        LV_ALIGN_TOP_MID,
        0,
        65
    );

    // =================================================
    // PIECE
    // =================================================

    label_piece = lv_label_create(screen_cycle);

    lv_label_set_text(
        label_piece,
        "Piece : 1 / 5"
    );

    lv_obj_set_style_text_font(
        label_piece,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_piece,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_piece,
        LV_ALIGN_TOP_MID,
        0,
        110
    );

    // =================================================
    // POSITION X
    // =================================================

    label_x_cycle = lv_label_create(screen_cycle);

    lv_label_set_text(
        label_x_cycle,
        "X : 0 mm"
    );

    lv_obj_set_style_text_font(
        label_x_cycle,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_x_cycle,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_x_cycle,
        LV_ALIGN_TOP_LEFT,
        250,
        155
    );

    // =================================================
    // POSITION Y
    // =================================================

    label_y_cycle = lv_label_create(screen_cycle);

    lv_label_set_text(
        label_y_cycle,
        "Y : 200 mm"
    );

    lv_obj_set_style_text_font(
        label_y_cycle,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        label_y_cycle,
        lv_color_black(),
        LV_PART_MAIN
    );

    lv_obj_align(
        label_y_cycle,
        LV_ALIGN_TOP_LEFT,
        450,
        155
    );

    // =================================================
    // BARRE DE PROGRESSION
    // =================================================

    bar_progression = lv_bar_create(screen_cycle);

    lv_obj_set_size(
        bar_progression,
        500,
        30
    );

    lv_obj_align(
        bar_progression,
        LV_ALIGN_CENTER,
        0,
        20
    );

    lv_bar_set_range(
        bar_progression,
        0,
        100
    );

    lv_bar_set_value(
        bar_progression,
        0,
        LV_ANIM_OFF
    );

    // =================================================
    // BOUTON PAUSE
    // =================================================

    btn_pause = lv_btn_create(screen_cycle);

    lv_obj_set_size(
        btn_pause,
        160,
        60
    );

    lv_obj_align(
        btn_pause,
        LV_ALIGN_BOTTOM_LEFT,
        180,
        -30
    );

    lv_obj_set_style_bg_color(
        btn_pause,
        lv_color_hex(0x607D8B),
        LV_PART_MAIN
    );

    label_pause = lv_label_create(btn_pause);

    lv_label_set_text(
        label_pause,
        "PAUSE"
    );

    lv_obj_set_style_text_font(
        label_pause,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_center(label_pause);

    // =================================================
    // BOUTON STOP
    // =================================================

    btn_stop = lv_btn_create(screen_cycle);

    lv_obj_set_size(
        btn_stop,
        160,
        60
    );

    lv_obj_align(
        btn_stop,
        LV_ALIGN_BOTTOM_RIGHT,
        -180,
        -30
    );

    lv_obj_set_style_bg_color(
        btn_stop,
        lv_color_hex(0xD32F2F),
        LV_PART_MAIN
    );

    label_stop = lv_label_create(btn_stop);

    lv_label_set_text(
        label_stop,
        "STOP"
    );

    lv_obj_set_style_text_font(
        label_stop,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_center(label_stop);

    // =================================================
    // CHARGER L'ECRAN
    // =================================================

    lv_scr_load(screen_cycle);
}
/* =====================================================
   SETUP
   ===================================================== */

void setup()
{
    Serial.begin(115200);


    /* =================================================
       I2C
       ================================================= */

    Wire.begin(8, 9);


    /* =================================================
       PCA9557
       ================================================= */

    Out.reset();

    Out.setMode(IO_OUTPUT);

    Out.setState(IO0, IO_HIGH);
    Out.setState(IO1, IO_HIGH);
    Out.setState(IO2, IO_HIGH);
    Out.setState(IO3, IO_HIGH);
    Out.setState(IO4, IO_HIGH);
    Out.setState(IO5, IO_HIGH);
    Out.setState(IO6, IO_HIGH);
    Out.setState(IO7, IO_HIGH);


    delay(1000);


    /* =================================================
       INITIALISATION TFT
       ================================================= */

    tft.begin();

    tft.fillScreen(TFT_BLACK);

    Serial.println("TFT_init");


    /* =================================================
       INITIALISATION LVGL
       ================================================= */

    lv_init();

    Serial.println("LV_init");


    lvgl_driver_function_init();


    /* =================================================
       ECRAN 1 : LOGO STELLANTIS
       ================================================= */

    lv_obj_t *screen = lv_scr_act();


    /* Fond noir */

    lv_obj_set_style_bg_color(
        screen,
        lv_color_black(),
        LV_PART_MAIN
    );


    /* Image */

    lv_obj_t *logo =
        lv_img_create(screen);


    /* Logo Stellantis */

    lv_img_set_src(
        logo,
        &stellantis_logo
    );


    /* Centrer */

    lv_obj_center(logo);


    /* =================================================
       PASSAGE AUTOMATIQUE A L'ECRAN 2
       ================================================= */

    lv_timer_create(
        [](lv_timer_t *timer)
        {
            create_screen_parametres();

            lv_timer_del(timer);
        },
        2500,
        NULL
    );
}


/* =====================================================
   LOOP PRINCIPALE
   ===================================================== */

void loop()
{
    lv_timer_handler();

    delay(5);
}














