// gtk_calculator.c 

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- 전역 계산 상태 변수 ---
static double current_value = 0.0;
static char current_operator = '\0';
static gboolean waiting_for_operand = FALSE;
static GtkWidget *display_label; 

// --- 함수 선언 ---
static void update_display(const char *text);
static void calculate_result();
static void button_clicked (GtkWidget *widget, gpointer data);
static void activate (GtkApplication *app, gpointer user_data);

// --- 계산 및 UI 업데이트 함수 ---

static void update_display(const char *text) {
    gtk_label_set_text(GTK_LABEL(display_label), text);
}

static void calculate_result() {
    double operand;
    const char *display_text = gtk_label_get_text(GTK_LABEL(display_label));
    operand = atof(display_text);

    switch (current_operator) {
        case '+': current_value += operand; break;
        case '-': current_value -= operand; break;
        case '*': current_value *= operand; break;
        case '/':
            if (operand != 0.0) {
                current_value /= operand;
            } else {
                update_display("오류: 0으로 나눌 수 없음");
                current_value = 0.0;
                current_operator = '\0';
                return;
            }
            break;
        case '\0':
            current_value = operand;
            break;
    }
    
    char result_str[50];
    sprintf(result_str, "%.10g", current_value);
    update_display(result_str);
}


// --- 버튼 클릭 콜백 함수 ---
static void button_clicked (GtkWidget *widget, gpointer data) {
    const char *label = gtk_button_get_label(GTK_BUTTON(widget));
    const char *display_text = gtk_label_get_text(GTK_LABEL(display_label));
    
    // 1. 숫자/소수점 입력 처리
    if (strspn(label, "0123456789.") == strlen(label)) { 
        if (waiting_for_operand || strcmp(display_text, "0") == 0 || strstr(display_text, "오류")) {
            // 소수점 중복 방지: 새 입력이 . 일 때, 기존에 .이 있다면 입력 방지
            if (strcmp(label, ".") == 0 && strchr(display_text, '.') != NULL && !waiting_for_operand) {
                return;
            }
            update_display(label);
            waiting_for_operand = FALSE;
        } else {
            // 소수점 중복 방지: 현재 숫자에 이미 .이 있을 경우 추가 입력 방지
            if (strcmp(label, ".") == 0 && strchr(display_text, '.') != NULL) {
                return;
            }
            
            char new_text[50];
            snprintf(new_text, sizeof(new_text), "%s%s", display_text, label);
            update_display(new_text);
        }
    } 
    // 2. 초기화 (C) 처리
    else if (strcmp(label, "C") == 0) {
        current_value = 0.0;
        current_operator = '\0';
        waiting_for_operand = FALSE;
        update_display("0");
    } 
    // 3. 연산자 (+, -, *, /) 처리
    else if (strspn(label, "+-*/") == strlen(label)) {
        if (!waiting_for_operand) {
             // 이전 계산을 수행하고 중간 결과를 저장
            calculate_result(); 
        } else {
            current_value = atof(display_text);
        }
        
        current_operator = label[0];
        waiting_for_operand = TRUE;
    } 
    // 4. 결과 (=) 처리: 최종 계산 및 연산자 초기화
    else if (strcmp(label, "=") == 0) {
        if (current_operator != '\0') {
            calculate_result();
            current_operator = '\0'; 
            waiting_for_operand = FALSE;
        }
    }
}


// --- GUI 설정 함수 (GTK 3) ---
static void activate (GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *button;

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "계산기 (GTK 3)");
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add (GTK_CONTAINER (window), grid); 
    
    display_label = gtk_label_new("0");
    gtk_widget_set_halign(display_label, GTK_ALIGN_END); 
    gtk_grid_attach (GTK_GRID (grid), display_label, 0, 0, 4, 1); 

    // --- '+' 버튼을 포함하도록 배열 수정 ---
    char *button_labels[] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+" // '+' 버튼이 올바른 위치에 추가됨
    };
    
    // 버튼 배치 로직은 4x4 배열 기준
    for (int i = 0; i < 16; i++) {
        button = gtk_button_new_with_label (button_labels[i]);
        g_signal_connect (button, "clicked", G_CALLBACK (button_clicked), NULL);
        
        int col = i % 4;
        int row = i / 4 + 1; 
        int width = 1; 
        
        gtk_grid_attach (GTK_GRID (grid), button, col, row, width, 1);
    }
    
    gtk_widget_show_all (window);
}

// --- main 함수 ---
int main (int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new ("org.gtk3.finalfixcalc", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}