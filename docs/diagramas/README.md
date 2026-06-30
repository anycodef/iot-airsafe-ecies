# Diagramas — AirSafe-ECIES

Esta carpeta contiene los diagramas del proyecto. Las imágenes PNG referenciadas en
el `README.md` y en la documentación se **generan con [diagrams.net (draw.io)](https://app.diagrams.net/)**.

## Archivos esperados

| Archivo | Descripción | Estado |
|---------|-------------|--------|
| `arquitectura-general.png` | Vista de los 3 planos (Edge → Gateway → Cloud) con tópicos MQTT y HTTPS. | ⏳ placeholder — generar con draw.io |
| `flujo-ecies.png` | Diagrama de secuencia del cifrado/descifrado ECIES (keygen → ECDH → KDF → AES). | ⏳ placeholder — generar con draw.io |

## Cómo generar los diagramas

1. Abre [diagrams.net](https://app.diagrams.net/).
2. Diseña el diagrama (o importa un `.drawio` editable que guardes en esta carpeta).
3. **Archivo → Exportar como → PNG** (fondo transparente, 2x de escala).
4. Guarda el PNG en `docs/diagramas/` con el nombre exacto de la tabla.
5. Conserva también el `.drawio` fuente para futuras ediciones.

## Contenido sugerido

**`arquitectura-general.png`**
- Tres cajas: ESP32-S3 (Edge), Raspberry Pi 4 (Gateway), Google Cloud (Sheets).
- Flechas etiquetadas: `MQTT cifrado (airsafe/telemetria, airsafe/alerta)` y `HTTPS POST`.
- Íconos de sensores/actuadores en el plano Edge.

**`flujo-ecies.png`**
- Diagrama de secuencia: Nodo ↔ Backend.
- Pasos: keygen efímero → ECDH → KDF SHA-256 → AES-128-CTR → envoltura Base64 → descifrado.
- Resaltar la propiedad de **Perfect Forward Secrecy** (clave efímera por mensaje).

> Mientras no existan los PNG, las imágenes embebidas en `README.md` se mostrarán como
> enlaces rotos; esto es esperado hasta que el equipo (Frente 4) genere los diagramas.
