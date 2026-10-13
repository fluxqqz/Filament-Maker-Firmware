#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>

// Web dashboard HTML / CSS / JS served to clients at "/"
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UPCYCLE Controller</title>

<style>
body{
font-family:Arial;
max-width:500px;
margin:auto;
padding:20px;
background:#111;
color:white;
}

.card{
background:#222;
padding:15px;
margin-bottom:15px;
border-radius:10px;
}

.step-card{
background:#222;
padding:15px;
margin-bottom:12px;
border-radius:10px;
border-left:4px solid #444;
}

.step-card.active{
border-left:4px solid #4caf50;
}

.temp-card{
background:linear-gradient(145deg,#1c1c1c,#242424);
padding:20px;
margin-bottom:15px;
border-radius:12px;
text-align:center;
border:1px solid #333;
}

.temp-main{
font-size:48px;
font-weight:bold;
color:#ff9800;
line-height:1;
}

.temp-sub{
margin-top:8px;
font-size:16px;
color:#ccc;
}

.temp-sub span{
font-weight:bold;
color:white;
}

.temp-pwm{
margin-top:6px;
font-size:13px;
color:#888;
}

.indicator-row{
display:flex;
gap:12px;
margin-top:14px;
}

.indicator{
flex:1;
padding:16px;
border-radius:10px;
text-align:center;
background:#1a1a1a;
border:2px solid #333;
transition:all 0.2s ease;
}

.indicator-label{
font-size:13px;
color:#aaa;
margin-bottom:8px;
text-transform:uppercase;
letter-spacing:1px;
}

.indicator-value{
font-size:20px;
font-weight:bold;
}

.big-dot{
display:inline-block;
width:16px;
height:16px;
border-radius:50%;
margin-right:8px;
vertical-align:middle;
}

.indicator.on{
border-color:#4caf50;
background:#16321a;
box-shadow:0 0 14px rgba(76,175,80,0.5);
}

.indicator.on .indicator-value{
color:#4caf50;
}

.indicator.on .big-dot{
background:#4caf50;
box-shadow:0 0 8px #4caf50;
animation:pulseGreen 1.4s infinite;
}

.indicator.off{
border-color:#444;
}

.indicator.off .indicator-value{
color:#f44336;
}

.indicator.off .big-dot{
background:#f44336;
}

@keyframes pulseGreen{
0%{box-shadow:0 0 6px rgba(76,175,80,0.6);}
50%{box-shadow:0 0 16px rgba(76,175,80,1);}
100%{box-shadow:0 0 6px rgba(76,175,80,0.6);}
}

button{
width:100%;
padding:15px;
margin-top:5px;
font-size:16px;
}

input{
width:100%;
padding:10px;
font-size:16px;
}

.status-on{
color:#4caf50;
font-weight:bold;
}

.status-off{
color:#f44336;
font-weight:bold;
}

.status-heating{
color:#ffb300;
font-weight:bold;
}

.dot{
display:inline-block;
width:10px;
height:10px;
border-radius:50%;
margin-right:6px;
}

.dot-on{
background:#4caf50;
}

.dot-off{
background:#f44336;
}

.dot-heating{
background:#ffb300;
}

.row{
display:flex;
gap:8px;
}

.row button{
flex:1;
}

.startBtn{
background:#2e7d32;
color:white;
border:none;
border-radius:6px;
}

.stopBtn{
background:#c62828;
color:white;
border:none;
border-radius:6px;
}

.spRow{
display:flex;
gap:8px;
margin-bottom:10px;
}

.spRow input{
flex:1;
}

.spRow button{
flex:0 0 auto;
width:auto;
padding:10px 14px;
margin-top:0;
}

label{
font-size:13px;
color:#aaa;
}
</style>
</head>

<body>

<h2>UPCYCLE Controller</h2>

<div class="temp-card">
<div class="temp-main"><span id="tempBig">0</span>&deg;C</div>
<div class="temp-sub">Setpoint: <span id="setpointBig">0</span>&deg;C</div>
<div class="temp-pwm">PWM: <span id="pwm">0</span></div>

<div class="indicator-row">
<div class="indicator off" id="heaterIndicator">
<div class="indicator-label">Heater</div>
<div class="indicator-value"><span class="big-dot" id="heaterBigDot"></span><span id="heaterBigText">OFF</span></div>
</div>
<div class="indicator off" id="stepperIndicator">
<div class="indicator-label">Stepper</div>
<div class="indicator-value"><span class="big-dot" id="stepperBigDot"></span><span id="stepperBigText">OFF</span></div>
</div>
</div>
</div>

<div class="card">
<h3>Process Steps</h3>

<div class="step-card" id="step1Card">
<div style="margin-bottom:8px;">
<span id="step1Dot" class="dot dot-off"></span>Step 1 - Heat: <span id="step1Status" class="status-off">OFF</span>
</div>

<label>Setpoint (&deg;C)</label>
<div class="spRow">
<input id="step1SPInput" type="number">
<button onclick="setStep1SP()">Set</button>
</div>

<div class="row">
<button class="startBtn" onclick="startStep1()">Start</button>
<button class="stopBtn" onclick="stopStep1()">Stop</button>
</div>
</div>

<div class="step-card" id="step2Card">
<div style="margin-bottom:8px;">
<span id="step2Dot" class="dot dot-off"></span>Step 2 - Heat: <span id="step2Status" class="status-off">OFF</span>
</div>

<label>Setpoint (&deg;C)</label>
<div class="spRow">
<input id="step2SPInput" type="number">
<button onclick="setStep2SP()">Set</button>
</div>

<div class="row">
<button class="startBtn" onclick="startStep2()">Start</button>
<button class="stopBtn" onclick="stopStep2()">Stop</button>
</div>
</div>

<div class="step-card" id="step3Card">
<div style="margin-bottom:8px;">
<span id="step3Dot" class="dot dot-off"></span>Step 3 - Stepper: <span id="step3Status" class="status-off">OFF</span>
</div>

<div class="row">
<button class="startBtn" onclick="startStep3()">Start</button>
<button class="stopBtn" onclick="stopStep3()">Stop</button>
</div>
</div>

</div>

<div class="card">
<h3>Stepper</h3>
<div>Wheel RPM: <span id="rpm">0</span></div>
</div>

<div class="card">
<h3>Manual Controls</h3>

<button onclick="heaterOn()">Heater ON</button>
<button onclick="heaterOff()">Heater OFF</button>

<br><br>

<button onclick="stepperOn()">Stepper ON</button>
<button onclick="stepperOff()">Stepper OFF</button>
</div>

<div class="card">
<h3>Set RPM (Output Wheel)</h3>

<input id="rpmInput" type="number">

<button onclick="setRPM()">
Update RPM
</button>
</div>

<script>

function heaterOn(){
fetch('/heater?state=on');
}

function heaterOff(){
fetch('/heater?state=off');
}

function stepperOn(){
fetch('/stepper?state=on');
}

function stepperOff(){
fetch('/stepper?state=off');
}

function startStep1(){
fetch('/step1?state=on');
}

function stopStep1(){
fetch('/step1?state=off');
}

function startStep2(){
fetch('/step2?state=on');
}

function stopStep2(){
fetch('/step2?state=off');
}

function startStep3(){
fetch('/step3?state=on');
}

function stopStep3(){
fetch('/step3?state=off');
}

function setStep1SP(){
let v = document.getElementById("step1SPInput").value;
fetch('/step1setpoint?value='+v);
}

function setStep2SP(){
let v = document.getElementById("step2SPInput").value;
fetch('/step2setpoint?value='+v);
}

function setRPM(){

let rpm =
document.getElementById("rpmInput").value;

fetch('/rpm?value='+rpm);
}

let spInputsInitialized = false;

setInterval(()=>{

fetch('/status')
.then(r=>r.json())
.then(data=>{

document.getElementById("tempBig").innerHTML =
data.temp;

document.getElementById("setpointBig").innerHTML =
data.setpoint;

document.getElementById("rpm").innerHTML =
data.rpm;

document.getElementById("pwm").innerHTML =
data.pwm;

if(!spInputsInitialized){
document.getElementById("step1SPInput").value = data.step1sp;
document.getElementById("step2SPInput").value = data.step2sp;
spInputsInitialized = true;
}

// Big Heater indicator
let heaterInd = document.getElementById("heaterIndicator");
let heaterTxt = document.getElementById("heaterBigText");
heaterInd.className = data.heater ? "indicator on" : "indicator off";
heaterTxt.innerHTML = data.heater ? "ON" : "OFF";

// Big Stepper indicator
let stepperInd = document.getElementById("stepperIndicator");
let stepperTxt = document.getElementById("stepperBigText");
stepperInd.className = data.stepper ? "indicator on" : "indicator off";
stepperTxt.innerHTML = data.stepper ? "ON" : "OFF";

// Step 1 status
let s1el = document.getElementById("step1Status");
let s1dot = document.getElementById("step1Dot");
let s1card = document.getElementById("step1Card");

if(data.step === 1 && data.heater){
s1card.className = "step-card active";
if(data.stable){
s1el.innerHTML = "Stable";
s1el.className = "status-on";
s1dot.className = "dot dot-on";
} else {
s1el.innerHTML = "Heating";
s1el.className = "status-heating";
s1dot.className = "dot dot-heating";
}
} else {
s1card.className = "step-card";
s1el.innerHTML = "OFF";
s1el.className = "status-off";
s1dot.className = "dot dot-off";
}

// Step 2 status
let s2el = document.getElementById("step2Status");
let s2dot = document.getElementById("step2Dot");
let s2card = document.getElementById("step2Card");

if(data.step === 2 && data.heater){
s2card.className = "step-card active";
if(data.stable){
s2el.innerHTML = "Stable";
s2el.className = "status-on";
s2dot.className = "dot dot-on";
} else {
s2el.innerHTML = "Heating";
s2el.className = "status-heating";
s2dot.className = "dot dot-heating";
}
} else {
s2card.className = "step-card";
s2el.innerHTML = "OFF";
s2el.className = "status-off";
s2dot.className = "dot dot-off";
}

// Step 3 status (stepper)
let s3el = document.getElementById("step3Status");
let s3dot = document.getElementById("step3Dot");
let s3card = document.getElementById("step3Card");

if(data.stepper){
s3card.className = "step-card active";
s3el.innerHTML = "Running";
s3el.className = "status-on";
s3dot.className = "dot dot-on";
} else {
s3card.className = "step-card";
s3el.innerHTML = "OFF";
s3el.className = "status-off";
s3dot.className = "dot dot-off";
}

});

},500);

</script>

</body>
</html>
)rawliteral";

#endif // DASHBOARD_H
