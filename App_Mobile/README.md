# AquaConscience – Backend

Node.js + Express + SQLite (archivo `aquaconscience.db`, se crea solo al primer arranque).

## Estructura
```
server.js        API REST
db.js            Esquema de la base de datos
public/          Tu frontend (index.html, css/styles.css, js/app.js)
```

## Ejecutar
```bash
npm install
cp .env.example .env      # y cambia JWT_SECRET
JWT_SECRET=tu_secreto npm start
# abre http://localhost:3000
```

## Base de datos
- **users**: id, name, email (único), password_hash (bcrypt, nunca en texto plano), created_at
- **households**: user_id, household (solo/familia), members, type (general/puntos)
- **sensors** (dispositivos): id, user_id, name, loc, api_key, created_at
- **readings** (consumo detectado): id, sensor_id, user_id, liters, activity, created_at

Al crear cuenta (`POST /api/auth/signup`) el usuario se guarda automáticamente en `users`.

## Endpoints
| Método | Ruta | Descripción |
|---|---|---|
| POST | /api/auth/signup | Crea usuario → `{token}` |
| POST | /api/auth/login | Inicia sesión → `{token}` |
| GET/PUT | /api/me | Perfil y hogar |
| POST | /api/onboarding | Guarda hogar y crea sensores (`demo:true` genera 7 días de datos de prueba) |
| GET | /api/dashboard | Resumen semanal, límites, alertas, recomendaciones |
| GET | /api/activities | Últimas lecturas |
| GET/POST/DELETE | /api/sensors | Gestión de dispositivos |
| POST | /api/readings | **Lo llama el dispositivo físico** |

## Enviar consumo desde un sensor real
```bash
curl -X POST http://TU_SERVIDOR/api/readings \
  -H "x-api-key: aqk_XXXX" -H "Content-Type: application/json" \
  -d '{"liters": 12.5, "activity": "Ducha"}'
```
La `api_key` de cada sensor se devuelve en `GET /api/sensors`.
Cuando uses sensores reales, quita `demo:true` en `finishOnboarding()` de `app.js`.

## Notas
- Límite recomendado: 100 L por persona al día (ajustable en `LIMIT_PER_PERSON_DAY`).
- Zona horaria de las gráficas: variable `TZ_APP` (por defecto America/Bogota).
