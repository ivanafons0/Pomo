#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

void init_database(sqlite3 **db){
    int control = sqlite3_open("pomDB", db);

    if(control!=SQLITE_OK)
    {
        fprintf(stderr,"Error al abrir base de datos: %s\n",sqlite3_errmsg(*db));
        return;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS sesiones ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "fecha DATE NOT NULL,"
                      "hora_inicio TIME NOT NULL,"
                      "hora_fin TIME,"
                      "ciclos_completados INTEGER NOT NULL,"
                      "duracion_trabajo INTEGER,"
                      "duracion_descanso INTEGER,"
                      "duracion_descanso_largo INTEGER"
                      ");";
    
    char *err_msg = NULL;
    control = sqlite3_exec(*db, sql, NULL, NULL, &err_msg);

    if (control != SQLITE_OK) {
        fprintf(stderr, "Error al crear tabla: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

}

void save_session(sqlite3 *db, time_t startTime, int cycles, int workedTime, int shortBreak, int longBreak)
{
    if(db == NULL)
    {
        return;
    }

    time_t endTime = time(NULL);
    struct tm *startTm = localtime (startTime);
    struct tm *endTm = localtime(&endTime);

    char fecha[20];
    char horaInicio[20];
    char horaFin[20];

    strftime(fecha, sizeof(fecha), "%Y-%m-%d", startTime);
    strftime(horaInicio, sizeof(horaInicio), "%H:%M:%S", startTm);
    strftime(horaFin, sizeof(horaFin), "%H:%M:%S", endTm);

    const char *sql = "INSERT INTO sesiones "
                      "(fecha, hora_inicio, hora_fin, ciclos_completados, "
                      "duracion_trabajo, duracion_descanso, duracion_descanso_largo) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int control = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
                      
    if (control == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, fecha, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, horaInicio, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, horaFin, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, cycles);
        sqlite3_bind_int(stmt, 5, workedTime);
        sqlite3_bind_int(stmt, 6, shortBreak);
        sqlite3_bind_int(stmt, 7, longBreak);

        control = sqlite3_step(stmt);

        if (control != SQLITE_DONE) {
            fprintf(stderr, "Error al guardar sesión: %s\n", sqlite3_errmsg(db));
        }
    } else {
        fprintf(stderr, "Error al preparar statement: %s\n", sqlite3_errmsg(db));
    }
    
    sqlite3_finalize(stmt);
}

void show_statistics(sqlite3 *db) {
    if (db == NULL) {
        printf("Base de datos no disponible.\n");
        return;
    }
    
    printf("\n╔════════════════════════════════════════════════╗\n");
    printf("║         ESTADÍSTICAS POMODORO                  ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");
    
    const char *sql_total = "SELECT COUNT(*), SUM(ciclos_completados) FROM sesiones;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql_total, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int total_sesiones = sqlite3_column_int(stmt, 0);
            int total_ciclos = sqlite3_column_int(stmt, 1);
            printf("  📊 Total de sesiones: %d\n", total_sesiones);
            printf("  🔄 Total de ciclos completados: %d\n\n", total_ciclos);
        }
    }
    sqlite3_finalize(stmt);
    
    const char *sql_mes = "SELECT COUNT(*), SUM(ciclos_completados) FROM sesiones "
                          "WHERE strftime('%Y-%m', fecha) = strftime('%Y-%m', 'now');";
    
    if (sqlite3_prepare_v2(db, sql_mes, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int sesiones_mes = sqlite3_column_int(stmt, 0);
            int ciclos_mes = sqlite3_column_int(stmt, 1);
            printf("  📅 Este mes:\n");
            printf("     - Sesiones: %d\n", sesiones_mes);
            printf("     - Ciclos: %d\n\n", ciclos_mes);
        }
    }
    sqlite3_finalize(stmt);
    
    const char *sql_ano = "SELECT COUNT(*), SUM(ciclos_completados) FROM sesiones "
                           "WHERE strftime('%Y', fecha) = strftime('%Y', 'now');";

    if (sqlite3_prepare_v2(db, sql_ano, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int sesiones_ano = sqlite3_column_int(stmt, 0);
            int ciclos_ano = sqlite3_column_int(stmt, 1);
            printf("  📆 Este año:\n");
            printf("     - Sesiones: %d\n", sesiones_ano);
            printf("     - Ciclos: %d\n\n", ciclos_ano);
        }
    }
    sqlite3_finalize(stmt);
    
    printf("  🕐 Últimas 5 sesiones:\n");
    const char *sql_ultimas = "SELECT fecha, hora_inicio, hora_fin, ciclos_completados "
                              "FROM sesiones ORDER BY id DESC LIMIT 5;";
    
    if (sqlite3_prepare_v2(db, sql_ultimas, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *fecha = (const char*)sqlite3_column_text(stmt, 0);
            const char *inicio = (const char*)sqlite3_column_text(stmt, 1);
            const char *fin = (const char*)sqlite3_column_text(stmt, 2);
            int ciclos = sqlite3_column_int(stmt, 3);
            
            printf("     - %s | %s - %s | %d ciclos\n", fecha, inicio, fin, ciclos);
        }
    }
    sqlite3_finalize(stmt);
    
    printf("\n");
}

const char *digits[10][3] = {
    {" ___ ", "|   |", "|___|"},  // 0
    {"     ", "    |", "    |"},  // 1
    {" ___ ", " ___|", "|___ "},  // 2
    {" ___ ", " ___|", " ___|"},  // 3
    {"     ", "|___|", "    |"},  // 4
    {" ___ ", "|___ ", " ___|"},  // 5
    {" ___ ", "|___ ", "|___|"},  // 6
    {" ___ ", "    |", "    |"},  // 7
    {" ___ ", "|___|", "|___|"},  // 8
    {" ___ ", "|___|", " ___|"}   // 9
};

const char *colon[3] = {
    "   ",
    " • ",
    " • "
};

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    keep_running = 0;
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void print_time_large(int minutes, int seconds) {
    int m1 = minutes / 10;
    int m2 = minutes % 10;
    int s1 = seconds / 10;
    int s2 = seconds % 10;
    
    // Imprimir las 3 líneas
    for (int line = 0; line < 3; line++) {
        printf("    %s  %s  %s  %s  %s\n", 
               digits[m1][line],
               digits[m2][line],
               colon[line],
               digits[s1][line],
               digits[s2][line]);
    }
}

void play_bell() {
    // Intentar reproducir sonido con varios métodos
    system("paplay /usr/share/sounds/freedesktop/stereo/complete.oga 2>/dev/null || \
            paplay /usr/share/sounds/freedesktop/stereo/bell.oga 2>/dev/null || \
            aplay /usr/share/sounds/alsa/Front_Center.wav 2>/dev/null || \
            beep 2>/dev/null || \
            printf '\\a'");
    fflush(stdout);
}

void show_notification(const char *title, const char *message) {
    char command[512];
    snprintf(command, sizeof(command), 
             "notify-send -u critical -i tomato '%s' '%s' 2>/dev/null", 
             title, message);
    system(command);
}

void countdown(int total_seconds, const char *phase) {
    while (total_seconds >= 0 && keep_running) {
        clear_screen();
        
        int mins = total_seconds / 60;
        int secs = total_seconds % 60;
        
        printf("\n\n");
        printf("  ╔════════════════════════════════════╗\n");
        printf("  ║     TEMPORIZADOR POMODORO          ║\n");
        printf("  ║     Fase: %-24s ║\n", phase);
        printf("  ╚════════════════════════════════════╝\n\n");
        
        print_time_large(mins, secs);
        
        printf("\n\n");
        printf("  Presiona Ctrl+C para detener\n");
        
        fflush(stdout);
        sleep(1);
        total_seconds--;
    }
    
    if (keep_running) {
        clear_screen();
        printf("\n\n");
        printf("  ╔════════════════════════════════════╗\n");
        printf("  ║   ¡TIEMPO COMPLETADO!              ║\n");
        printf("  ╚════════════════════════════════════╝\n\n");
        
        // Mostrar notificación del sistema
        char notification_msg[256];
        if (strcmp(phase, "TRABAJO") == 0) {
            snprintf(notification_msg, sizeof(notification_msg), 
                     "¡Tiempo de descanso! Has completado una sesión de trabajo.");
            show_notification("Pom - Trabajo completado", notification_msg);
        } else if (strcmp(phase, "DESCANSO CORTO") == 0) {
            snprintf(notification_msg, sizeof(notification_msg), 
                     "¡Descanso terminado! Es hora de volver al trabajo.");
            show_notification("Pom - Descanso completado", notification_msg);
        } else if (strcmp(phase, "DESCANSO LARGO") == 0) {
            snprintf(notification_msg, sizeof(notification_msg), 
                     "¡Descanso largo terminado! ¿Listo para otro ciclo?");
            show_notification("Pom - Descanso largo completado", notification_msg);
        }
        
        // Sonar 3 veces
        for (int i = 0; i < 3; i++) {
            play_bell();
            usleep(300000);
        }
        
        sleep(2);
    }
}

void print_usage(const char *prog_name) {
    printf("\nUso: %s [opciones]\n\n", prog_name);
    printf("Opciones:\n");
    printf("  -w, --work <mins>     Duración del trabajo (por defecto: 25 min)\n");
    printf("  -b, --break <mins>    Duración del descanso corto (por defecto: 5 min)\n");
    printf("  -l, --long <mins>     Duración del descanso largo (por defecto: 15 min)\n");
    printf("  -c, --cycles <num>    Número de ciclos antes del descanso largo (por defecto: 4)\n");
    printf("  -s, --stats            Mostrar estadísticas de sesiones anteriores\n");
    printf("  -h, --help            Mostrar esta ayuda\n\n");
    printf("Ejemplos:\n");
    printf("  %s                    Temporizador Pomodoro estándar (25/5/15)\n", prog_name);
    printf("  %s -w 50 -b 10        Sesiones de 50 minutos con descansos de 10\n", prog_name);
    printf("  %s -c 2               Solo 2 ciclos antes del descanso largo\n\n", prog_name);
    printf("  %s -s                 Ver estadísticas de todas tus sesiones \n\n", prog_name);
}

int main(int argc, char *argv[]) {
    int work_time = 25;
    int short_break = 5;
    int long_break = 15;
    int cycles = 4;
    int show_stats_only = 0;

    sqlite3 *db = NULL;
    init_database(&db);
    
    // Procesar argumentos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--work") == 0) {
            if (i + 1 < argc) work_time = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--break") == 0) {
            if (i + 1 < argc) short_break = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--long") == 0) {
            if (i + 1 < argc) long_break = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cycles") == 0) {
            if (i + 1 < argc) cycles = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--stats") == 0) {
            show_stats_only = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if(show_stats_only){
        show_statistics(db);
        sqlite3_close(db);
        return 0;
    }
    
    // Configurar manejador de señales
    signal(SIGINT, handle_sigint);
    
    printf("\n🍅 TEMPORIZADOR POMODORO 🍅\n");
    printf("═══════════════════════════\n");
    printf("Trabajo: %d min | Descanso: %d min | Descanso largo: %d min\n", 
           work_time, short_break, long_break);
    printf("Ciclos antes del descanso largo: %d\n\n", cycles);
    printf("Presiona Enter para comenzar...");
    getchar();
    
    time_t sesStart = time(NULL);
    int cycle = 1;
    
    while (keep_running) {
        // Fase de trabajo
        countdown(work_time * 60, "TRABAJO");
        if (!keep_running) break;
        
        // Determinar tipo de descanso
        if (cycle % cycles == 0) {
            countdown(long_break * 60, "DESCANSO LARGO");
        } else {
            countdown(short_break * 60, "DESCANSO CORTO");
        }
        
        if (!keep_running) break;
        
        cycle++;
        
        // Preguntar si continuar
        clear_screen();
        printf("\n¿Continuar con el siguiente ciclo? (s/n): ");
        char response;
        scanf(" %c", &response);
        if (response != 's' && response != 'S') break;
    }
    
     int completed_cycles = cycle - 1;
    if (completed_cycles > 0) {
        save_session(db, sesStart, completed_cycles, 
                     work_time, short_break, long_break);
    }
    
    clear_screen();
    printf("\n¡Sesión de Pomodoro finalizada! \n");
    printf("Completaste %d ciclo(s).\n", completed_cycles);
    printf("Sesión guardada en la base de datos.\n\n");
    
    // Mostrar estadísticas rápidas
    printf("Escribe '%s -s' para ver todas tus estadísticas.\n\n", argv[0]);
    
    sqlite3_close(db);
    return 0;
}
