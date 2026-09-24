let state = null;
const KEY = 'aquaconscience_state_v1';

/* ---------- Persistencia ---------- */
function load(){ try{ const r = localStorage.getItem(KEY); return r? JSON.parse(r): null; }catch(e){ return null; } }
function save(){ try{ localStorage.setItem(KEY, JSON.stringify(state)); }catch(e){} }
function toast(msg){ const t=document.getElementById('toast'); t.textContent=msg; t.classList.add('show'); setTimeout(()=>t.classList.remove('show'),1800); }

/* ---------- Datos de ejemplo ---------- */
function seedData(household, members, type){
  const perPerson = 130; // litros/día recomendados por persona (referencia informativa)
  const people = household==='solo'?1:(members||3);
  const dailyLimit = perPerson*people;
  const weeklyLimit = dailyLimit*7;
  const days = ['L','M','X','J','V','S','D'];
  const week = days.map(d=>({d, l: Math.round(dailyLimit*(0.6+Math.random()*0.7))}));
  const sensors = type==='general'
    ? [{id:1,name:'Sensor principal',loc:'Tubería general'}]
    : [{id:1,name:'Cocina',loc:'Cocina'},{id:2,name:'Baño principal',loc:'Baño'},{id:3,name:'Lavandería',loc:'Lavandería'}];
  const acts = [
    {who:sensors[0].name, l: Math.round(dailyLimit*0.18), t:'Hoy · 8:12 a.m.'},
    {who:sensors[Math.min(1,sensors.length-1)].name, l: Math.round(dailyLimit*0.32), t:'Hoy · 7:40 a.m.'},
    {who:sensors[0].name, l: Math.round(dailyLimit*0.22), t:'Ayer · 9:05 p.m.'},
    {who:sensors[Math.min(2,sensors.length-1)].name, l: Math.round(dailyLimit*0.15), t:'Ayer · 6:30 p.m.'},
  ];
  return {household, members:people, type, dailyLimit, weeklyLimit, week, sensors, activities:acts, nextSensorId: sensors.length+1};
}

/* ---------- AUTH ---------- */
function setAuthTab(t){
  document.getElementById('tab-login').classList.toggle('on', t==='login');
  document.getElementById('tab-signup').classList.toggle('on', t==='signup');
  document.getElementById('form-login').style.display = t==='login'?'block':'none';
  document.getElementById('form-signup').style.display = t==='signup'?'block':'none';
  document.getElementById('auth-err').style.display='none';
}
function showErr(msg){ const e=document.getElementById('auth-err'); e.textContent=msg; e.style.display='block'; }
function doLogin(){
  const email=document.getElementById('li-email').value.trim();
  const pass=document.getElementById('li-pass').value;
  if(!email||!pass){ showErr('Ingresa tu correo y contraseña.'); return; }
  const existing = load();
  if(existing && existing.user && existing.user.email===email){
    state = existing; goApp();
  } else {
    showErr('No encontramos esa cuenta. Crea una nueva para probar la app.');
  }
}
function doSignup(){
  const name=document.getElementById('su-name').value.trim();
  const email=document.getElementById('su-email').value.trim();
  const pass=document.getElementById('su-pass').value;
  if(!name||!email||pass.length<6){ showErr('Completa todos los campos (contraseña de 6+ caracteres).'); return; }
  state = { user:{name,email}, onboarded:false };
  save();
  document.getElementById('screen-auth').classList.remove('active');
  document.getElementById('screen-onboarding').classList.add('active');
}

/* ---------- ONBOARDING ---------- */
let obHousehold=null, obType=null;
function selectHousehold(v, el){
  obHousehold=v;
  document.querySelectorAll('#ob-step1 .optcard').forEach(c=>c.classList.remove('sel'));
  el.classList.add('sel');
  document.getElementById('members-wrap').style.display = v==='familia'?'block':'none';
}
function goStep2(){
  if(!obHousehold){ toast('Selecciona una opción'); return; }
  document.getElementById('ob-step1').style.display='none';
  document.getElementById('ob-step2').style.display='block';
}
function backStep1(){
  document.getElementById('ob-step2').style.display='none';
  document.getElementById('ob-step1').style.display='block';
}
function selectType(v, el){
  obType=v;
  document.querySelectorAll('#ob-step2 .optcard').forEach(c=>c.classList.remove('sel'));
  el.classList.add('sel');
}
function finishOnboarding(){
  if(!obType){ toast('Selecciona el tipo de instalación'); return; }
  const members = parseInt(document.getElementById('ob-members').value)||3;
  const seed = seedData(obHousehold, members, obType);
  state = { ...state, ...seed, onboarded:true };
  save();
  document.getElementById('screen-onboarding').classList.remove('active');
  goApp();
}

/* ---------- APP RENDER ---------- */
function goApp(){
  document.querySelectorAll('.screen').forEach(s=>s.classList.remove('active'));
  document.getElementById('screen-app').classList.add('active');
  renderAll();
}
function initials(name){ return (name||'?').split(' ').map(w=>w[0]).slice(0,2).join('').toUpperCase(); }

function renderAll(){
  const u=state.user;
  document.getElementById('app-name').textContent = u.name.split(' ')[0];
  document.getElementById('app-avatar').textContent = initials(u.name);
  document.getElementById('pf-avatar').textContent = initials(u.name);
  document.getElementById('pf-name').textContent = u.name;
  document.getElementById('pf-sub').textContent = (state.household==='solo'?'Hogar unipersonal':'Núcleo familiar · '+state.members+' personas') + ' · ' + u.email;

  const total = state.week.reduce((a,d)=>a+d.l,0);
  document.getElementById('home-total').textContent = total.toLocaleString('es')+' L';
  const diff = Math.round((total-state.weeklyLimit)/state.weeklyLimit*100);
  document.getElementById('home-vs').textContent = diff<=0
    ? `${Math.abs(diff)}% por debajo del límite recomendado`
    : `${diff}% por encima del límite recomendado`;

  const maxL = Math.max(...state.week.map(d=>d.l), state.dailyLimit);
  document.getElementById('home-bars').innerHTML = state.week.map(d=>{
    const h = Math.max(6, Math.round(d.l/maxL*90));
    const over = d.l>state.dailyLimit;
    return `<div class="bar-col"><div class="bar ${over?'over':''}" style="height:${h}px"></div><span>${d.d}</span></div>`;
  }).join('');

  const overLimit = total>state.weeklyLimit;
  document.getElementById('home-alert').innerHTML = overLimit ? `
    <div class="alert"><div class="ic">⚠️</div><div>
      <h3>Consumo por encima del límite</h3>
      <p>Superaste tu límite semanal recomendado (${state.weeklyLimit.toLocaleString('es')} L). Revisa la pestaña de Tips para reducirlo.</p>
    </div></div>` : '';

  const actHtml = state.activities.map(a=>{
    const over = a.l > state.dailyLimit*0.3;
    return `<div class="actrow"><div><div class="who">${a.who}</div><div class="meta">${a.t}</div></div><div class="lit ${over?'over':''}">${a.l} L</div></div>`;
  }).join('');
  document.getElementById('home-activity').innerHTML = actHtml;
  document.getElementById('activity-list').innerHTML = actHtml || '<p style="font-size:13px;color:#5A716C;">Aún no hay actividad registrada.</p>';

  const recos = buildRecos();
  document.getElementById('reco-list').innerHTML = recos.map(r=>`<div class="reco"><div class="dot"></div><div><h3>${r.t}</h3><p>${r.d}</p></div></div>`).join('');

  document.getElementById('sensor-list').innerHTML = state.sensors.map(s=>`
    <div class="sensor"><div><div class="name">${s.name}</div><div class="loc">${s.loc}</div></div>
    <button class="iconbtn" onclick="removeSensor(${s.id})">Eliminar</button></div>`).join('') ||
    '<p style="font-size:13px;color:#5A716C;">No tienes sensores registrados.</p>';
}

function buildRecos(){
  const base = [
    {t:'Revisa fugas silenciosas', d:'Un goteo constante puede desperdiciar hasta 30 litros al día sin que lo notes.'},
    {t:'Duchas de 5 minutos', d:'Reducir tu ducha en 2 minutos puede ahorrar hasta 20 litros por baño.'},
    {t:'Lavadora a carga completa', d:'Usa la lavadora solo con carga completa para aprovechar mejor el agua por ciclo.'},
  ];
  if(state.household!=='solo') base.push({t:'Reparte el consumo', d:'Con '+state.members+' personas en casa, coordinar horarios de ducha ayuda a distribuir mejor el uso.'});
  if(state.type==='puntos') base.push({t:'Compara tus puntos', d:'Revisa cuál sensor consume más esta semana y ajusta los hábitos en esa zona.'});
  return base;
}

/* ---------- NAV ---------- */
function showView(v){
  ['home','activity','reco','profile'].forEach(n=>{
    document.getElementById('view-'+n).classList.toggle('active', n===v);
    document.getElementById('nav-'+n).classList.toggle('on', n===v);
  });
}

/* ---------- PROFILE ---------- */
function openEditProfile(){
  document.getElementById('ep-name').value=state.user.name;
  document.getElementById('ep-email').value=state.user.email;
  document.getElementById('ep-household').value=state.household;
  document.getElementById('ep-members').value=state.members;
  document.getElementById('modal-profile').classList.add('open');
}
function saveProfile(){
  state.user.name = document.getElementById('ep-name').value.trim() || state.user.name;
  state.user.email = document.getElementById('ep-email').value.trim() || state.user.email;
  state.household = document.getElementById('ep-household').value;
  state.members = parseInt(document.getElementById('ep-members').value)||1;
  save(); closeModal('modal-profile'); renderAll(); toast('Perfil actualizado');
}
function closeModal(id){ document.getElementById(id).classList.remove('open'); }

/* ---------- SENSORS ---------- */
function openAddSensor(){ document.getElementById('as-name').value=''; document.getElementById('modal-sensor').classList.add('open'); }
function addSensor(){
  const name=document.getElementById('as-name').value.trim();
  const loc=document.getElementById('as-loc').value;
  if(!name){ toast('Escribe un nombre para el sensor'); return; }
  state.sensors.push({id:state.nextSensorId++, name, loc});
  save(); closeModal('modal-sensor'); renderAll(); toast('Sensor registrado');
}
function removeSensor(id){
  state.sensors = state.sensors.filter(s=>s.id!==id);
  save(); renderAll(); toast('Sensor eliminado');
}

function logout(){
  document.querySelectorAll('.screen').forEach(s=>s.classList.remove('active'));
  document.getElementById('screen-auth').classList.add('active');
  document.getElementById('li-email').value=''; document.getElementById('li-pass').value='';
}

/* ---------- INIT ---------- */
(function init(){
  const existing = load();
  if(existing && existing.onboarded){ state = existing; goApp(); }
})();
