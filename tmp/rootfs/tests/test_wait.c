#include "nano_libc.h"

int main(int argc, char* argv[]) {
    nano_print("=== [TEST AVANSAT] Fork + Float + Wait ===\n");
    
    int pid = fork();
    
    if (pid < 0) {
        nano_print("Eroare la fork!\n");
        nano_sys_exit(1);
    } 
    else if (pid == 0) {
        // ==========================================
        // PROCESUL COPIL
        // ==========================================
        nano_print("[COPIL] Am pornit cu succes. Execut un calcul cu virgula mobila...\n");
        
        // Testăm operare pe flotoane (compilatorul va folosi registrele XMM)
        float val1 = 15.5f;
        float val2 = 2.5f;
        float rezultat = val1 * val2; // 38.75
        
        if (rezultat > 38.0f && rezultat < 39.0f) {
            nano_print("[COPIL] Calcul float VALIDAT! Registrele XMM functioneaza perfect.\n");
        } else {
            nano_print("[COPIL] Eroare la calculul float!\n");
        }
        
        nano_print("[COPIL] Îmi închei treaba și ies curat.\n");
        nano_sys_exit(0);
    } 
    else {
        // ==========================================
        // PROCESUL PĂRINTE
        // ==========================================
        nano_print("[PARINTE] Am creat copilul cu PID-ul: ");
        nano_print_int(pid);
        nano_print("\n");
        
        nano_print("[PARINTE] Folosesc nano_wait() pentru a prelua starea copilului...\n");
        
        // Părinte adoarme (PROC_SLEEPING) până când copilul dă exit() și devine ZOMBIE
        int finished_pid = nano_wait();
        
        nano_print("[PARINTE] Trezit din somn! Copilul cu PID-ul ");
        nano_print_int(finished_pid);
        nano_print(" a fost cules cu succes.\n");
        
        nano_print("[PARINTE] Test completat cu brio! La revedere.\n");
        nano_sys_exit(0);
    }
    
    return 0;
}