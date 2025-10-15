const functions = require('firebase-functions');
const admin = require('firebase-admin');
admin.initializeApp();

const strefa = 'Europe/Warsaw';

exports.agregujDziennie = functions.pubsub
  .schedule('30 0 * * *')
  .timeZone(strefa)
  .onRun(async () => {
    const db = admin.database();
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

      const min = (arr) => Math.min(...arr);
      const max = (arr) => Math.max(...arr);
      const avg = (arr) => +(arr.reduce((a, b) => a + b, 0) / arr.length).toFixed(1);

      const dataISO = wczoraj.toISOString().split('T')[0];

      const zestawienie = {
        data: dataISO,
        temperatura: {
          min: min(temps),
          max: max(temps),
          srednia: avg(temps),
        },
        wilgotnosc: {
          min: min(wilgs),
          max: max(wilgs),
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
    return null;
  });