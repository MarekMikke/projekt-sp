const axios = require("axios");

// Konfiguracja
const CZUJNIK_ID = "ESP_Bot";
const FIREBASE_URL = "https://projekt-sp-default-rtdb.europe-west1.firebasedatabase.app";

// Funkcja generująca dane i wysyłająca do Firebase
function wyslijDane() {
  const temp = +(20 + Math.random() * 5).toFixed(1);
  const wilg = +(40 + Math.random() * 10).toFixed(1);
  const { DateTime } = require("luxon");
  const timestamp = DateTime.now().setZone("Europe/Warsaw").toISO();

  const payload = {
    timestamp,
    temperatura: temp,
    wilgotnosc: wilg
  };

  const url = `${FIREBASE_URL}/czujniki/${CZUJNIK_ID}.json`;

  axios.post(url, payload)
    .then(() => console.log(`[${timestamp}] Wysłano: ${temp} °C, ${wilg} %`))
    .catch(err => console.error("Błąd:", err.response?.data || err.message));
}


wyslijDane();
setInterval(wyslijDane, 10 * 1000);