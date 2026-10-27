#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#define POP_SIZE 50
#define CROSSOVER_PROB 0.90
#define MUTATION_PROB 0.01   /* 1% por hijo. Cambiar a 0.10 si el profesor pide 10%. */
#define X_MIN (-5.12)
#define X_MAX ( 5.12)
#define PI 3.14159265358979323846

#define MASK_X 0xFFFFFFFF00000000ULL
#define MASK_Y 0x00000000FFFFFFFFULL

typedef struct {
    uint64_t xy;      /* bits 63..32 = x, bits 31..0 = y */
    double x;
    double y;
    double fitness;   /* Rastrigin: menor es mejor */
} Individuo;

static double aleatorio_01(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}

/* rand() no garantiza 32 bits; se construyen con dos bloques de 16 bits. */
static uint32_t aleatorio_u32(void) {
    uint32_t alto = (uint32_t)(rand() & 0xFFFF);
    uint32_t bajo = (uint32_t)(rand() & 0xFFFF);
    return (alto << 16) | bajo;
}

static uint64_t crear_xy(uint32_t x_bits, uint32_t y_bits) {
    return ((uint64_t)x_bits << 32) | (uint64_t)y_bits;
}

static uint32_t bits_x(uint64_t xy) {
    return (uint32_t)(xy >> 32);
}

static uint32_t bits_y(uint64_t xy) {
    return (uint32_t)(xy & 0xFFFFFFFFULL);
}

/*
 * X = A + [valor_decimal / (2^L - 1)] * (B - A)
 * Para L=32, 2^32-1 = UINT32_MAX.
 */
static double decodificar(uint32_t gen) {
    return X_MIN + ((double)gen / (double)UINT32_MAX) * (X_MAX - X_MIN);
}

static double rastrigin(double x, double y) {
    return 20.0
         + x*x - 10.0*cos(2.0*PI*x)
         + y*y - 10.0*cos(2.0*PI*y);
}

static void evaluar(Individuo *ind) {
    ind->x = decodificar(bits_x(ind->xy));
    ind->y = decodificar(bits_y(ind->xy));
    ind->fitness = rastrigin(ind->x, ind->y);
}

static void inicializar_poblacion(Individuo poblacion[POP_SIZE]) {
    for (int i = 0; i < POP_SIZE; ++i) {
        uint32_t gx = aleatorio_u32();
        uint32_t gy = aleatorio_u32();
        poblacion[i].xy = crear_xy(gx, gy);
        evaluar(&poblacion[i]);
    }
}

/*
 * Cruza en la mitad de los 64 bits:
 * hijo1 = X del padre1 + Y del padre2
 * hijo2 = X del padre2 + Y del padre1
 *
 * Los AND con MASK_X/MASK_Y extraen las mitades y OR las combina.
 */
static void cruzar(uint64_t p1, uint64_t p2, uint64_t *h1, uint64_t *h2) {
    if (aleatorio_01() < CROSSOVER_PROB) {
        *h1 = (p1 & MASK_X) | (p2 & MASK_Y);
        *h2 = (p2 & MASK_X) | (p1 & MASK_Y);
    } else {
        *h1 = p1;
        *h2 = p2;
    }
}

/*
 * Mutación del 1% por hijo: si ocurre, se invierte un bit aleatorio.
 * XOR con una máscara de un solo bit cambia 0->1 o 1->0.
 */
static void mutar(uint64_t *xy) {
    if (aleatorio_01() < MUTATION_PROB) {
        unsigned int posicion = (unsigned int)(rand() % 64);
        uint64_t mascara = 1ULL << posicion;
        *xy ^= mascara;
    }
}

/* Ejemplo directo de AND para consultar un bit de un entero de 32 bits. */
static int obtener_bit32(uint32_t valor, unsigned int posicion) {
    if (posicion >= 32) {
        return -1;
    }
    uint32_t mascara = 1U << posicion;
    return (valor & mascara) ? 1 : 0;
}

static int comparar_individuos(const void *a, const void *b) {
    const Individuo *ia = (const Individuo *)a;
    const Individuo *ib = (const Individuo *)b;

    if (ia->fitness < ib->fitness) return -1;
    if (ia->fitness > ib->fitness) return 1;
    return 0;
}

static void imprimir_binario64(uint64_t valor) {
    for (int bit = 63; bit >= 0; --bit) {
        putchar((valor & (1ULL << bit)) ? '1' : '0');
        if (bit == 32) {
            printf(" | ");
        }
    }
}

static void generar_hijos(const Individuo poblacion[POP_SIZE],
                          Individuo hijos[POP_SIZE]) {
    for (int i = 0; i < POP_SIZE; i += 2) {
        int a = rand() % POP_SIZE;
        int b = rand() % POP_SIZE;

        while (b == a) {
            b = rand() % POP_SIZE;
        }

        uint64_t h1, h2;
        cruzar(poblacion[a].xy, poblacion[b].xy, &h1, &h2);

        mutar(&h1);
        mutar(&h2);

        hijos[i].xy = h1;
        evaluar(&hijos[i]);

        if (i + 1 < POP_SIZE) {
            hijos[i + 1].xy = h2;
            evaluar(&hijos[i + 1]);
        }
    }
}

/*
 * Reemplazo elitista:
 * 50 padres + 50 hijos = 100 candidatos.
 * Se ordenan por Rastrigin ascendente y sobreviven los mejores 50.
 */
static void seleccionar_mejores(Individuo poblacion[POP_SIZE],
                                const Individuo hijos[POP_SIZE]) {
    Individuo candidatos[POP_SIZE * 2];

    for (int i = 0; i < POP_SIZE; ++i) {
        candidatos[i] = poblacion[i];
        candidatos[POP_SIZE + i] = hijos[i];
    }

    qsort(candidatos,
          POP_SIZE * 2,
          sizeof(Individuo),
          comparar_individuos);

    for (int i = 0; i < POP_SIZE; ++i) {
        poblacion[i] = candidatos[i];
    }
}

int main(void) {
    int generaciones;
    Individuo poblacion[POP_SIZE];
    Individuo hijos[POP_SIZE];

    srand((unsigned int)time(NULL));

    printf("Algoritmo genetico - Rastrigin de dos variables\n");
    printf("Poblacion: %d individuos\n", POP_SIZE);
    printf("Cruza: %.0f%%\n", CROSSOVER_PROB * 100.0);
    printf("Mutacion: %.0f%% por hijo\n", MUTATION_PROB * 100.0);
    printf("Ingrese el numero de generaciones: ");

    if (scanf("%d", &generaciones) != 1 || generaciones <= 0) {
        fprintf(stderr, "Numero de generaciones invalido.\n");
        return EXIT_FAILURE;
    }

    inicializar_poblacion(poblacion);
    qsort(poblacion, POP_SIZE, sizeof(Individuo), comparar_individuos);

    for (int g = 0; g < generaciones; ++g) {
        generar_hijos(poblacion, hijos);
        seleccionar_mejores(poblacion, hijos);

        if (g == 0 || (g + 1) % 100 == 0 || g + 1 == generaciones) {
            printf("Generacion %5d | mejor f(x,y) = %.12f | x = %.9f | y = %.9f\n",
                   g + 1,
                   poblacion[0].fitness,
                   poblacion[0].x,
                   poblacion[0].y);
        }
    }

    printf("\nMEJOR INDIVIDUO\n");
    printf("===============================================\n");
    printf("xy binario : ");
    imprimir_binario64(poblacion[0].xy);
    printf("\n");
    printf("x bits     : 0x%08" PRIX32 " (%" PRIu32 ")\n",
           bits_x(poblacion[0].xy), bits_x(poblacion[0].xy));
    printf("y bits     : 0x%08" PRIX32 " (%" PRIu32 ")\n",
           bits_y(poblacion[0].xy), bits_y(poblacion[0].xy));
    printf("x real     : %.12f\n", poblacion[0].x);
    printf("y real     : %.12f\n", poblacion[0].y);
    printf("Rastrigin  : %.12f\n", poblacion[0].fitness);

    printf("\nEjemplo AND: bit 3 del gen X = %d\n",
           obtener_bit32(bits_x(poblacion[0].xy), 3));

    return EXIT_SUCCESS;
}
