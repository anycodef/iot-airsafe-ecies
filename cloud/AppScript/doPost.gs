/**
 * doPost.gs — Endpoint de ingesta de AirSafe-ECIES.
 *
 * Recibe la telemetría YA DESCIFRADA desde Node-RED (HTTPS POST con JSON)
 * y la agrega como una fila a la Google Sheet activa.
 *
 * Despliegue:
 *   Extensiones → Apps Script → pegar este archivo →
 *   Implementar → Nueva implementación → Aplicación web
 *     · Ejecutar como: yo
 *     · Quién tiene acceso: cualquiera
 *   Copiar la URL .../exec en el nodo "http request" de Node-RED.
 */

// Nombre de la hoja donde se escriben los datos.
var SHEET_NAME = 'Telemetria';

// Encabezados (se crean automáticamente si la hoja está vacía).
var HEADERS = [
  'timestamp', 'temp', 'humedad', 'presion',
  'mq2_ppm', 'mq135_ppm', 'alerta_gas'
];

/**
 * Maneja las peticiones POST de Node-RED.
 * @param {Object} e Evento con e.postData.contents (JSON).
 * @return {ContentService.TextOutput} Respuesta JSON { status }.
 */
function doPost(e) {
  try {
    if (!e || !e.postData || !e.postData.contents) {
      return jsonResponse({ status: 'error', message: 'cuerpo vacío' });
    }

    var data = JSON.parse(e.postData.contents);
    var sheet = getSheet_();

    sheet.appendRow([
      data.timestamp || new Date().toISOString(),
      numOrEmpty_(data.temp),
      numOrEmpty_(data.humedad),
      numOrEmpty_(data.presion),
      numOrEmpty_(data.mq2_ppm),
      numOrEmpty_(data.mq135_ppm),
      data.alerta_gas === true
    ]);

    return jsonResponse({ status: 'ok' });
  } catch (err) {
    return jsonResponse({ status: 'error', message: String(err) });
  }
}

/** Permite verificar el despliegue desde el navegador (GET). */
function doGet() {
  return jsonResponse({ status: 'ok', service: 'AirSafe-ECIES ingest' });
}

/** Obtiene (o crea) la hoja de telemetría con sus encabezados. */
function getSheet_() {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sheet = ss.getSheetByName(SHEET_NAME);
  if (!sheet) {
    sheet = ss.insertSheet(SHEET_NAME);
  }
  if (sheet.getLastRow() === 0) {
    sheet.appendRow(HEADERS);
  }
  return sheet;
}

/** Devuelve el número o cadena vacía si el valor no es válido. */
function numOrEmpty_(v) {
  return (typeof v === 'number' && !isNaN(v)) ? v : '';
}

/** Empaqueta un objeto como respuesta JSON de Apps Script. */
function jsonResponse(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
