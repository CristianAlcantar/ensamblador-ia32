#ifndef ENSAMBLADOR_IA32_HPP
#define ENSAMBLADOR_IA32_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>

using namespace std;

// --- ESTRUCTURAS DE DATOS ---
// Estructura para almacenar una referencia pendiente
struct ReferenciaPendiente {
    int posicion; // Posición en codigo_hex donde se necesita el parche
    int tamano_inmediato; // 1 o 4 (byte o dword para el desplazamiento)
    int tipo_salto; // 0: Absoluto (dirección de etiqueta), 1: Relativo (dirección de salto)
};

class EnsambladorIA32 {
public:
    // Constructor
    EnsambladorIA32();

    // Función principal para iniciar el ensamblado
    void ensamblar(const string& archivo_entrada);

    // Función para resolver las referencias a etiquetas
    void resolver_referencias_pendientes();

    // Función para generar el código máquina en hexadecimal
    void generar_hex(const string& archivo_salida);

    // Función para generar los reportes de las tablas
    void generar_reportes();

private:
    // Tablas de ensamblado requeridas
    unordered_map<string, int> tabla_simbolos; [cite_start]// Etiqueta -> Dirección (Contador de Posición) [cite: 7]
    unordered_map<string, vector<ReferenciaPendiente>> referencias_pendientes; [cite_start]// Etiqueta -> Lista de posiciones a parchear [cite: 8]
    vector<uint8_t> codigo_hex; [cite_start]// Vector para el código máquina generado [cite: 9]
    int contador_posicion; // Equivalente al Location Counter

    // Mapas para codificación
    unordered_map<string, uint8_t> reg32_map; // Códigos de 32-bit (EAX=0, ECX=1, ...)
    unordered_map<string, uint8_t> reg8_map; // Códigos de 8-bit

    // --- FUNCIONES DE SOPORTE ---
    void inicializar_mapas();
    void limpiar_linea(string& linea);
    bool es_etiqueta(const string& s);
    void procesar_etiqueta(const string& etiqueta);
    void procesar_linea(string linea);

    // --- FUNCIONES DE PROCESAMIENTO DE INSTRUCCIONES ---
    void procesar_instruccion(const string& linea);
    void procesar_mov(const string& operandos);
    void procesar_add(const string& operandos);
    void procesar_sub(const string& operandos); // Nueva instrucción
    void procesar_jmp(const string& operandos);
    void procesar_condicional(const string& mnem, const string& operandos);

    // --- UTILIDADES DE CODIFICACIÓN ---
    uint8_t generar_modrm(uint8_t mod, uint8_t reg, uint8_t rm);
    void agregar_byte(uint8_t byte);
    void agregar_dword(uint32_t dword); // Para inmediatos y desplazamientos de 32 bits
    bool obtener_reg32(const string& op, uint8_t& reg_code);
    bool procesar_mem_simple(const string& operando, uint8_t& modrm_byte, const uint8_t reg_code, bool es_destino, uint8_t op_extension = 0);
};

#endif // ENSAMBLADOR_IA32_HPP