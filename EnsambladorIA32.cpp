#include "EnsambladorIA32.hpp"

// -----------------------------------------------------------------------------
// 📌 Inicialización
// -----------------------------------------------------------------------------

EnsambladorIA32::EnsambladorIA32() : contador_posicion(0) {
    inicializar_mapas();
}

void EnsambladorIA32::inicializar_mapas() {
    // Codificación de registros de 32 bits (REG/R/M)
    reg32_map = {
        {"EAX", 0b000}, {"ECX", 0b001}, {"EDX", 0b010}, {"EBX", 0b011},
        {"ESP", 0b100}, {"EBP", 0b101}, {"ESI", 0b110}, {"EDI", 0b111}
    };
    // Codificación de registros de 8 bits
    reg8_map = {
        {"AL", 0b000}, {"CL", 0b001}, {"DL", 0b010}, {"BL", 0b011},
        {"AH", 0b100}, {"CH", 0b101}, {"DH", 0b110}, {"BH", 0b111}
    };
}

// -----------------------------------------------------------------------------
// 🧹 Utilidades
// -----------------------------------------------------------------------------

void EnsambladorIA32::limpiar_linea(string& linea) {
    // Quitar comentarios
    size_t pos = linea.find(';');
    if (pos != string::npos) linea = linea.substr(0, pos);

    // Eliminar espacios en blanco al inicio y al final
    linea.erase(linea.begin(), find_if(linea.begin(), linea.end(), [](unsigned char ch) {
        return !isspace(ch);
    }));
    linea.erase(find_if(linea.rbegin(), linea.rend(), [](unsigned char ch) {
        return !isspace(ch);
    }).base(), linea.end());

    // Convertir a mayúsculas para facilitar la comparación
    transform(linea.begin(), linea.end(), linea.begin(), ::toupper);
}

bool EnsambladorIA32::es_etiqueta(const string& s) {
    // Una etiqueta termina con ':'
    return !s.empty() && s.back() == ':';
}

uint8_t EnsambladorIA32::generar_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    // Estructura: Mod(7-6) | Reg/Opcode(5-3) | [cite_start]R/M(2-0) [cite: 52]
    return (mod << 6) | (reg << 3) | rm;
}

void EnsambladorIA32::agregar_byte(uint8_t byte) {
    codigo_hex.push_back(byte);
    contador_posicion += 1;
}

void EnsambladorIA32::agregar_dword(uint32_t dword) {
    [cite_start]// Little-endian para valores de 32 bits [cite: 37]
    agregar_byte(static_cast<uint8_t>(dword & 0xFF));
    agregar_byte(static_cast<uint8_t>((dword >> 8) & 0xFF));
    agregar_byte(static_cast<uint8_t>((dword >> 16) & 0xFF));
    agregar_byte(static_cast<uint8_t>((dword >> 24) & 0xFF));
}

bool EnsambladorIA32::obtener_reg32(const string& op, uint8_t& reg_code) {
    if (reg32_map.count(op)) {
        reg_code = reg32_map.at(op);
        return true;
    }
    return false;
}

// Maneja la codificación de memoria simple (etiqueta a 32-bit address)
bool EnsambladorIA32::procesar_mem_simple(const string& operando, uint8_t& modrm_byte, const uint8_t reg_code, bool es_destino, uint8_t op_extension) {
    // Patrón para una etiqueta simple: [ETIQUETA]
    if (operando.size() > 2 && operando.front() == '[' && operando.back() == ']') {
        string etiqueta = operando.substr(1, operando.size() - 2);

        // Mod = 00, R/M = 101 (indica desplazamiento de 32 bits, sin registro base)
        uint8_t mod = 0b00;
        uint8_t rm = 0b101;
        
        // reg_code es el registro o la extensión de opcode
        uint8_t reg_field = es_destino ? op_extension : reg_code; 
        
        modrm_byte = generar_modrm(mod, reg_field, rm);

        agregar_byte(modrm_byte); // Agrega ModR/M

        if (tabla_simbolos.count(etiqueta)) {
            // Etiqueta resuelta: agrega la dirección absoluta
            agregar_dword(tabla_simbolos.at(etiqueta));
        } else {
            // Etiqueta pendiente: agrega una referencia y llena con ceros
            referencias_pendientes[etiqueta].push_back({contador_posicion, 4, 0}); // 4 bytes, Tipo 0: Absoluto
            agregar_dword(0x00000000);
        }
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 🧠 Procesamiento del Ensamblado (Pasada Única)
// -----------------------------------------------------------------------------

void EnsambladorIA32::procesar_etiqueta(const string& etiqueta) {
    [cite_start]// La etiqueta se almacena con la posición actual del Contador de Posición [cite: 689]
    tabla_simbolos[etiqueta] = contador_posicion;
}

void EnsambladorIA32::procesar_instruccion(const string& linea) {
    stringstream ss(linea);
    string mnem;
    ss >> mnem;

    string resto;
    getline(ss, resto);

    if (mnem == "MOV") {
        procesar_mov(resto);
    } else if (mnem == "ADD") {
        procesar_add(resto);
    } else if (mnem == "SUB") {
        procesar_sub(resto);
    } else if (mnem == "JMP") {
        procesar_jmp(resto);
    } 
    // ... Las condicionales se agrupan
    else if (mnem == "JE" || mnem == "JNE" || mnem == "JLE" || mnem == "JL" || mnem == "JZ" || mnem == "JNZ" || mnem == "JA" || mnem == "JAE" || mnem == "JB" || mnem == "JBE" || mnem == "JG" || mnem == "JGE") {
        procesar_condicional(mnem, resto);
    } else if (mnem == "INT") {
        // Implementación simple INT imm8 (CD imm8)
        stringstream imm_ss(resto);
        string imm_str;
        imm_ss >> imm_str;
        try {
            uint32_t immediate = stoul(imm_str, nullptr, 16);
            agregar_byte(0xCD);
            agregar_byte(static_cast<uint8_t>(immediate));
        } catch (...) {
            cerr << "Error: Inmediato INT no válido." << endl;
        }
    } else {
        cerr << "Error: Instrucción no implementada o no válida: " << mnem << endl;
    }
}

void EnsambladorIA32::procesar_linea(string linea) {
    limpiar_linea(linea);
    if (linea.empty()) return;

    if (es_etiqueta(linea)) {
        procesar_etiqueta(linea.substr(0, linea.size() - 1));
        return;
    }

    procesar_instruccion(linea);
}

// -----------------------------------------------------------------------------
// ⚙️ Lógica de Instrucciones
// -----------------------------------------------------------------------------

void EnsambladorIA32::procesar_mov(const string& operandos) {
    stringstream ss(operandos);
    string dest_str, src_str;
    getline(ss, dest_str, ',');
    getline(ss, src_str);

    limpiar_linea(dest_str);
    limpiar_linea(src_str);

    uint8_t dest_code, src_code;
    bool dest_is_reg = obtener_reg32(dest_str, dest_code);
    bool src_is_reg = obtener_reg32(src_str, src_code);
    
    // ... Lógica MOV (simplificada para concisión) ...

    // 1. MOV Registro-Registro (MOV EAX, EBX)
    if (dest_is_reg && src_is_reg) {
        [cite_start]// Opcode 89 (r/m32, r32) si se usa el registro de destino como r/m y fuente como reg [cite: 35]
        agregar_byte(0x89);
        agregar_byte(generar_modrm(0b11, src_code, dest_code)); [cite_start]// Mod=11 (Reg-Reg) [cite: 778]
        return;
    }
    
    // 2. MOV Inmediato-Registro (MOV EAX, 1234h)
    if (dest_is_reg && !src_is_reg && src_str.find_first_not_of("0123456789ABCDEFH") == string::npos && src_str.back() == 'H') {
        try {
            uint32_t immediate = stoul(src_str.substr(0, src_str.size()-1), nullptr, 16);
            agregar_byte(0xB8 + dest_code); [cite_start]// B8+rd [cite: 35]
            agregar_dword(immediate);
            return;
        } catch (...) {}
    }
    
    // 3. MOV Memoria-Registro (MOV [ETIQUETA], EAX)
    if (src_is_reg && dest_str.size() > 2 && dest_str.front() == '[' && dest_str.back() == ']') {
        agregar_byte(0x89); [cite_start]// Opcode 89 (r/m32, r32) [cite: 35]
        uint8_t modrm_byte;
        if (procesar_mem_simple(dest_str, modrm_byte, src_code, true)) return;
    }
    
    // 4. MOV Registro-Memoria (MOV EAX, [ETIQUETA])
    if (dest_is_reg && src_str.size() > 2 && src_str.front() == '[' && src_str.back() == ']') {
        agregar_byte(0x8B); [cite_start]// Opcode 8B (r32, r/m32) [cite: 35]
        uint8_t modrm_byte;
        if (procesar_mem_simple(src_str, modrm_byte, dest_code, false)) return;
    }
    
    cerr << "Error de sintaxis o modo no soportado para MOV: " << operandos << endl;
}

void EnsambladorIA32::procesar_add(const string& operandos) {
    // ... Lógica ADD (similar a MOV) ...
    // Solo como ejemplo: ADD EAX, EBX -> 01 D8
    // ADD EAX, [ETIQUETA] -> 03 05 addr
    // ADD [ETIQUETA], EAX -> 01 05 addr
    // ADD EAX, 1234H -> 05 34 12 00 00
    cerr << "Lógica ADD no implementada completamente en este ejemplo." << endl;
}

void EnsambladorIA32::procesar_sub(const string& operandos) {
    stringstream ss(operandos);
    string dest_str, src_str;
    getline(ss, dest_str, ',');
    getline(ss, src_str);

    limpiar_linea(dest_str);
    limpiar_linea(src_str);

    uint8_t dest_code, src_code;
    bool dest_is_reg = obtener_reg32(dest_str, dest_code);
    bool src_is_reg = obtener_reg32(src_str, src_code);

    // 1. SUB Registro-Registro (SUB EBX, EAX)
    if (dest_is_reg && src_is_reg) {
        // Opcode: 29 (SUB r/m32, r32). [cite_start]Destino es r/m, Fuente es Reg [cite: 71]
        agregar_byte(0x29);
        agregar_byte(generar_modrm(0b11, src_code, dest_code));
        return;
    }
    
    // 2. SUB Inmediato-Registro (SUB EAX, 1234h) - Caso especial EAX
    if (dest_is_reg && dest_str == "EAX" && !src_is_reg) {
        [cite_start]// Opcode 2D (SUB EAX, imm32) [cite: 80]
        try {
            uint32_t immediate = stoul(src_str.substr(0, src_str.size()-1), nullptr, 16);
            agregar_byte(0x2D); 
            agregar_dword(immediate);
            return;
        } catch (...) {}
    }
    
    // 3. SUB Inmediato-Memoria (SUB [ETIQUETA], 12h) - Asumiendo imm8 (sign-extended)
    if (dest_str.size() > 2 && dest_str.front() == '[' && dest_str.back() == ']' && !src_is_reg) {
        try {
            uint32_t immediate = stoul(src_str.substr(0, src_str.size()-1), nullptr, 16);
            if (immediate <= 0xFF) {
                [cite_start]// Opcode: 83 (/5 para SUB) (SUB r/m32, imm8) [cite: 84, 85]
                agregar_byte(0x83);
                uint8_t op_extension = 0b101; [cite_start]// /5 para SUB [cite: 85]
                uint8_t modrm_byte;
                
                if (procesar_mem_simple(dest_str, modrm_byte, 0, true, op_extension)) {
                    agregar_byte(static_cast<uint8_t>(immediate)); // Agrega imm8
                    return;
                }
            }
        } catch (...) {}
    }
    
    cerr << "Error de sintaxis o modo no soportado para SUB: " << operandos << endl;
}

void EnsambladorIA32::procesar_jmp(const string& operandos) {
    // JMP near (32-bit relativo)
    limpiar_linea(operandos);
    string etiqueta = operandos;

    agregar_byte(0xE9); [cite_start]// Opcode E9 [cite: 89]
    
    int tamano_instruccion = 5; // E9 (1 byte) + desplazamiento (4 bytes)
    int posicion_referencia = contador_posicion;
    contador_posicion += 4; // Avanzar el contador de posición para el desplazamiento (4 bytes)

    if (tabla_simbolos.count(etiqueta)) {
        // Etiqueta resuelta: calcular y agregar desplazamiento relativo
        int destino = tabla_simbolos.at(etiqueta);
        int offset = destino - (posicion_referencia + 4); // Desplazamiento = Destino - (Dirección Siguiente Instrucción)
        agregar_dword(static_cast<uint32_t>(offset));
    } else {
        // Etiqueta pendiente: agregar referencia y llenar con ceros
        referencias_pendientes[etiqueta].push_back({posicion_referencia, 4, 1}); // 4 bytes, Tipo 1: Relativo
        agregar_dword(0x00000000);
    }
}

void EnsambladorIA32::procesar_condicional(const string& mnem, const string& operandos) {
    // Se asume el salto largo (near, 32-bit relativo)
    limpiar_linea(operandos);
    string etiqueta = operandos;
    
    uint8_t opcode_byte1 = 0x0F;
    uint8_t opcode_byte2;

    [cite_start]// Mapeo de mnemónicos condicionales a su segundo byte de opcode (salto largo OF 8X) [cite: 98, 105, 108, 111, 114, 117, 120, 125, 131, 136]
    if (mnem == "JE" || mnem == "JZ") opcode_byte2 = 0x84;
    else if (mnem == "JNE" || mnem == "JNZ") opcode_byte2 = 0x85;
    else if (mnem == "JLE") opcode_byte2 = 0x8E;
    else if (mnem == "JL") opcode_byte2 = 0x8C;
    else if (mnem == "JA") opcode_byte2 = 0x87;
    else if (mnem == "JAE") opcode_byte2 = 0x83;
    else if (mnem == "JB") opcode_byte2 = 0x82;
    else if (mnem == "JBE") opcode_byte2 = 0x86;
    else if (mnem == "JG") opcode_byte2 = 0x8F;
    else if (mnem == "JGE") opcode_byte2 = 0x8D;
    else return;

    agregar_byte(opcode_byte1);
    agregar_byte(opcode_byte2);

    int posicion_referencia = contador_posicion;
    contador_posicion += 4; // 4 bytes para el desplazamiento relativo

    if (tabla_simbolos.count(etiqueta)) {
        int destino = tabla_simbolos.at(etiqueta);
        int offset = destino - (posicion_referencia + 4); 
        agregar_dword(static_cast<uint32_t>(offset));
    } else {
        referencias_pendientes[etiqueta].push_back({posicion_referencia, 4, 1}); // Tipo 1: Relativo
        agregar_dword(0x00000000);
    }
}

// -----------------------------------------------------------------------------
// 🔨 Segunda Lógica de Pasada (Parcheo de Referencias)
// -----------------------------------------------------------------------------

void EnsambladorIA32::resolver_referencias_pendientes() {
    for (auto& par : referencias_pendientes) {
        const string& simbolo = par.first;
        const vector<ReferenciaPendiente>& posiciones = par.second;

        if (tabla_simbolos.count(simbolo)) {
            int destino = tabla_simbolos.at(simbolo); 

            for (const auto& ref : posiciones) {
                int pos = ref.posicion;
                uint32_t valor_a_parchear;

                if (ref.tipo_salto == 0) {
                    // Tipo 0: Absoluto (dirección de memoria)
                    valor_a_parchear = destino;
                } else if (ref.tipo_salto == 1) {
                    // Tipo 1: Relativo (Dirección de Destino - Dirección Siguiente Instrucción)
                    int offset = destino - (pos + ref.tamano_inmediato);
                    valor_a_parchear = static_cast<uint32_t>(offset);
                }

                // Parchear los bytes en el código hexadecimal (little-endian)
                if (ref.tamano_inmediato == 4) {
                    codigo_hex[pos] = static_cast<uint8_t>(valor_a_parchear & 0xFF);
                    codigo_hex[pos + 1] = static_cast<uint8_t>((valor_a_parchear >> 8) & 0xFF);
                    codigo_hex[pos + 2] = static_cast<uint8_t>((valor_a_parchear >> 16) & 0xFF);
                    codigo_hex[pos + 3] = static_cast<uint8_t>((valor_a_parchear >> 24) & 0xFF);
                }
            }
        } else {
            cerr << "Advertencia: Etiqueta no definida '" << simbolo << "'. Referencia no resuelta." << endl;
        }
    }
}

// -----------------------------------------------------------------------------
// 🏁 Funciones de Ensamblado y Reporte
// -----------------------------------------------------------------------------

void EnsambladorIA32::ensamblar(const string& archivo_entrada) {
    ifstream f(archivo_entrada);
    if (!f.is_open()) {
        cerr << "Error: no se pudo abrir " << archivo_entrada << endl;
        return;
    }

    string linea;
    while (getline(f, linea)) {
        procesar_linea(linea);
    }
}

void EnsambladorIA32::generar_hex(const string& archivo_salida) {
    ofstream f(archivo_salida);
    if (!f.is_open()) {
        cerr << "Error: no se pudo crear el archivo de salida " << archivo_salida << endl;
        return;
    }
    
    f << std::hex << std::uppercase << std::setfill('0');
    
    for (uint8_t byte : codigo_hex) {
        f << std::setw(2) << static_cast<int>(byte) << " "; 
    }
    f << endl;
}

void EnsambladorIA32::generar_reportes() {
    // Generar Tabla de Símbolos
    ofstream simb("simbolos.txt");
    simb << "Tabla de Símbolos:" << endl;
    for (const auto& par : tabla_simbolos) {
        simb << par.first << ": 0x" << std::hex << std::uppercase << par.second << endl;
    }

    [cite_start]// Generar Tabla de Referencias Pendientes [cite: 637]
    ofstream refs("referencias.txt");
    refs << "Tabla de Referencias Pendientes:" << endl;
    for (const auto& par : referencias_pendientes) {
        refs << "Etiqueta: " << par.first << endl;
        for (const auto& ref : par.second) {
            refs << "  -> Posición: 0x" << std::hex << ref.posicion
                 << ", Tamaño: " << std::dec << ref.tamano_inmediato
                 << " bytes, Tipo: " << ref.tipo_salto << endl;
        }
    }
}

// -----------------------------------------------------------------------------
// 🚀 Función Principal
// -----------------------------------------------------------------------------

int main() {
    // 1. Crear un archivo de ejemplo para probar el ensamblador (programa.asm)
    ofstream asm_file("programa.asm");
    asm_file << "; Ejemplo de código ensamblador IA-32" << endl;
    asm_file << "SECTION .TEXT" << endl;
    asm_file << "GLOBAL _START" << endl;
    asm_file << "_START:" << endl; // 0x00
    asm_file << "MOV EAX, 1234H" << endl; // B8 34 12 00 00 (5 bytes)
    asm_file << "MOV EBX, EAX" << endl; // 89 C3 (2 bytes)
    asm_file << "SUB EBX, EAX" << endl; // 29 C3 (2 bytes)
    asm_file << "JMP ETIQUETA_FIN" << endl; // E9 xx xx xx xx (5 bytes)
    asm_file << "LOOP_START:" << endl;
    asm_file << "MOV [VAR_DATA], EAX" << endl; // 89 05 xx xx xx xx (6 bytes)
    asm_file << "SUB [VAR_DATA], 0AH" << endl; // 83 2D xx xx xx xx 0A (7 bytes)
    asm_file << "JMP LOOP_START" << endl; // E9 xx xx xx xx (5 bytes)
    asm_file << "ETIQUETA_FIN:" << endl;
    asm_file << "INT 80H" << endl; // CD 80 (2 bytes)
    asm_file << "SECTION .DATA" << endl;
    asm_file << "VAR_DATA: DD 0" << endl; // Etiqueta definida al final (dirección de VAR_DATA)
    asm_file.close();

    // 2. Ejecutar el ensamblador
    EnsambladorIA32 ensamblador;
    cout << "Iniciando ensamblado en una sola pasada (EnsambladorIA32.cpp)..." << endl;
    ensamblador.ensamblar("programa.asm");
    
    cout << "Resolviendo referencias pendientes..." << endl;
    ensamblador.resolver_referencias_pendientes();
    
    cout << "Generando código hexadecimal y reportes..." << endl;
    ensamblador.generar_hex("programa.hex");
    ensamblador.generar_reportes();
    
    cout << "Proceso completado. Revise programa.hex, simbolos.txt y referencias.txt" << endl;

    return 0;
}