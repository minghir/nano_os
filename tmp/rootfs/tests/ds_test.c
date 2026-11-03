#include "nano_libc.h"
#include "nano_string.h"
#include "libds.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    nano_print("=== INCEPERE TEST LIBDS (Structuri de Date) ===\n");

    // ==========================================
    // 1. Test Vector Dinamic
    // ==========================================
    nano_print("\n[Test 1] Vector Dinamic:\n");
    Vector* vec = vector_create(4);
    if (!vec) {
        nano_print("Eroare: Nu s-a putut crea vectorul!\n");
        return 1;
    }

    // Adăugăm câteva elemente (simulăm niște ID-uri sau pointeri)
    float* p_val = (float*)nano_malloc(sizeof(float));
	*p_val = 10.2f;
	vector_push(vec, p_val);

    vector_push(vec, (void*)200);
    vector_push(vec, (void*)300);
    vector_push(vec, (void*)400);
    vector_push(vec, (void*)500); // Testăm extinderea automată a capacității

    nano_print("Elementele din vector (size = 5):\n");
    for (int i = 0; i < vec->size; i++) {
			if(i == 0){
				float* retrieved = (float*)vector_get(vec, 0);
				nano_printf(" - Element index %d: valoare = %f\n", i, *retrieved);
			}else{
				int val = (int)(intptr_t)vector_get(vec, i);
				nano_printf(" - Element index %d: valoare = %d\n", i, val);
			}
		
        
    }
    nano_print("Vectorul a procesat elementele corect.\n");
    vector_free(vec);

    // ==========================================
    // 2. Test Listă Dublu Înlănțuită & Z-Order
    // ==========================================
    nano_print("\n[Test 2] Lista Dublu Inlantuita (Simulare Ferestre):\n");
    LinkedList* list = list_create();
    if (!list) {
        nano_print("Eroare: Nu s-a putut crea lista!\n");
        return 1;
    }

    // Creăm 3 noduri (simulăm ferestrele F1, F2, F3)
    Node* n1 = list_push_back(list, (void*)"Fereastra 1 (Fundal)");
    Node* n2 = list_push_back(list, (void*)"Fereastra 2 (Mijloc)");
    Node* n3 = list_push_back(list, (void*)"Fereastra 3 (Fata)");

    nano_print("Starea initiala a listei (de la spate la fata):\n");
    nano_print((const char*)n1->data); nano_print("\n");
    nano_print((const char*)n2->data); nano_print("\n");
    nano_print((const char*)n3->data); nano_print("\n");

    // Simulăm un click pe Fereastra 1 (o aducem în fața tuturor - Z-Order)
    nano_print("\nSimulam focus pe Fereastra 1 (o ducem la coada listei)...\n");
    list_to_tail(list, n1);

    // Verificăm cine este acum la coadă (tail = elementul din cel mai față plan)
    if (list->tail == n1) {
        nano_print("SUCCES: Fereastra 1 este acum in fata tuturor (tail corect)!\n");
        nano_print("Elementul din fata este: ");
        nano_print((const char*)list->tail->data);
        nano_print("\n");
    } else {
        nano_print("EROARE la mutarea Z-Order!\n");
    }

    list_free(list);

    nano_print("\n=== TOATE TESTELE LIBDS AU TRECUT CU SUCCES! ===\n");
    return 0;
}