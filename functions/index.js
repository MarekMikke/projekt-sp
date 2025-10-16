const { onSchedule } = require('firebase-functions/v2/scheduler');
const { initializeApp } = require('firebase-admin/app');
const { getDatabase } = require('firebase-admin/database');

initializeApp();

const strefa = 'Europe/Warsaw';

exports.agregujDziennie = onSchedule(
  {
    schedule: '30 0 * * *',
    timeZone: strefa,
  },
  async (event) => {
    const db = getDatabase();
    const czujnikiSnap = await db.ref('czujniki').once('value');
    const czujniki = czujnikiSnap.val();

    const dzis = new Date();
    dzis.setHours(0, 0, 0, 0);
    const wczoraj = new Date(dzis.getTime() - 86400000);
    const tydzienTemu = new Date(dzis.getTime() - 7 * 86400000);

    for (const id in czujniki) {
      if (!Object.prototype.hasOwnProperty.call(czujniki, id)) continue;

      const pomiary = Object.entries(czujniki[id])
        .map(([key, p]) => ({ ...p, _id: key }))
        .filter((p) => {
          const t = new Date(p.timestamp);
          return t >= wczoraj && t < dzis;
        });

      if (pomiary.length === 0) continue;

      const temps = pomiary.map((p) => p.temperatura);
      const wilgs = pomiary.map((p) => p.wilgotnosc);

      const findExtreme = (arr, key, mode) => {
        const sorted = [...arr].sort((a, b) => mode === 'min' ? a[key] - b[key] : b[key] - a[key]);
        const best = sorted[0];
        const godzina = new Date(best.timestamp).toISOString().split('T')[1].slice(0, 5); // "HH:MM"
        return {
          wartosc: best[key],
          godzina,
        };
      };
      const avg = (arr) => +(arr.reduce((a, b) => a + b, 0) / arr.length).toFixed(1);

      const dataISO = wczoraj.toISOString().split('T')[0];

      const zestawienie = {
        data: dataISO,
        temperatura: {
          min: temperaturaMin.wartosc,
          min_godzina: temperaturaMin.godzina,
          max: temperaturaMax.wartosc,
          max_godzina: temperaturaMax.godzina,
          srednia: avg(temps),
        },
        wilgotnosc: {
          min: wilgotnoscMin.wartosc,
          min_godzina: wilgotnoscMin.godzina,
          max: wilgotnoscMax.wartosc,
          max_godzina: wilgotnoscMax.godzina,
          srednia: avg(wilgs),
        },
      };

      await db.ref(`zestawienia/${id}/${dataISO}`).set(zestawienie);

      const doUsuniecia = Object.entries(czujniki[id])
        .filter(([, p]) => {
          const t = new Date(p.timestamp);
          return t < tydzienTemu;
        });

      for (const entry of doUsuniecia) {
        const key = entry[0];
        await db.ref(`czujniki/${id}/${key}`).remove();
      }
    }

    console.log('✔️ Agregacja zakończona');
  }
);