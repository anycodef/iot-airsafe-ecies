/**
 * settings.js — Configuración de Node-RED para AirSafe-ECIES.
 *
 * Expone el módulo `crypto` de Node.js al contexto global de las funciones
 * (necesario para el nodo "ECIES Decrypt") y define el puerto/credenciales.
 *
 * La clave privada del backend NO se escribe aquí: se lee de la variable de
 * entorno BACKEND_PRIV_KEY_HEX (inyectada por docker-compose / .env).
 */
module.exports = {
    // ─── Servidor del editor / runtime ───
    uiPort: process.env.PORT || 1880,
    flowFile: "flows.json",
    flowFilePretty: true,

    // ─── Contexto global disponible en los nodos function ───
    // Permite: const crypto = global.get('crypto')
    functionGlobalContext: {
        crypto: require("crypto"),
    },

    // ─── Seguridad del editor (descomenta y genera el hash con bcrypt) ───
    // adminAuth: {
    //     type: "credentials",
    //     users: [{
    //         username: "admin",
    //         password: "$2b$08$REEMPLAZAR_HASH_BCRYPT",
    //         permissions: "*"
    //     }]
    // },

    // Clave para cifrar credenciales almacenadas en el flujo.
    credentialSecret: process.env.NODE_RED_CREDENTIAL_SECRET || "airsafe-dev-secret",

    // ─── Logging ───
    logging: {
        console: {
            level: "info",
            metrics: false,
            audit: false,
        },
    },

    // ─── Editor ───
    editorTheme: {
        projects: { enabled: false },
        page: { title: "AirSafe-ECIES — Gateway" },
    },
};
