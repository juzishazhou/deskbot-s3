#include "hw/webui.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "hw/display.h"

namespace {

constexpr const char *kApSsid = "Robot-Z";
constexpr const char *kApPassword = "12345678";

WebServer gServer(80);

// 网页请求只做“记一笔”，动作由主循环执行，避免在 HTTP 回调里阻塞
bool gPendingValid = false;
RobotAction gPendingAction = RobotAction::Reset;

const char kIndexHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0e1116">
<title>Robot-Z 控制台</title>
<style>
:root{--bg:#0e1116;--card:#161b22;--line:#242c37;--fg:#e8eef7;--dim:#8b98a9;--accent:#4f8cff;--ok:#3ecf8e}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;padding:20px 16px 36px;background:radial-gradient(120% 80% at 50% 0%,#1a2330 0%,var(--bg) 62%);color:var(--fg);font:16px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI","PingFang SC","Microsoft YaHei",sans-serif;min-height:100vh}
header{max-width:520px;margin:0 auto 20px;text-align:center}
h1{font-size:20px;margin:0 0 6px;letter-spacing:.5px}
.sub{color:var(--dim);font-size:13px;margin:0}
.card{max-width:520px;margin:0 auto 16px;background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px}
.card h2{font-size:12px;color:var(--dim);font-weight:600;letter-spacing:1.4px;margin:0 0 12px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button{appearance:none;border:1px solid var(--line);background:#1d2430;color:var(--fg);border-radius:14px;min-height:74px;padding:8px 4px;font:600 16px/1.2 inherit;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;transition:transform .1s,background .15s,border-color .15s;cursor:pointer}
button i{font:400 11px/1 inherit;color:var(--dim);font-style:normal}
button:active{transform:scale(.96);background:#243040;border-color:var(--accent)}
button.done{border-color:var(--ok);background:#17301f}
.toast{position:fixed;left:50%;bottom:26px;transform:translate(-50%,16px);padding:10px 18px;border-radius:999px;background:#243040;border:1px solid var(--line);color:var(--fg);font-size:14px;opacity:0;transition:opacity .2s,transform .2s;pointer-events:none;max-width:86vw}
.toast.show{opacity:1;transform:translate(-50%,0)}
footer{max-width:520px;margin:24px auto 0;text-align:center;color:var(--dim);font-size:12px}
</style>
</head>
<body>
<header>
<h1>Robot-Z</h1>
<p class="sub">ESP32-S3 桌面机器人 · 手机连热点后即可遥控</p>
</header>
<section class="card">
<h2>表情</h2>
<div class="grid">
<button data-face="normal">正常<i>平静脸</i></button>
<button data-face="happy">开心<i>笑眼</i></button>
<button data-face="blink">眨眼<i>眨一下</i></button>
</div>
</section>
<section class="card">
<h2>动作</h2>
<div class="grid">
<button data-action="nod">点头<i>低头再抬</i></button>
<button data-action="shake">摇头<i>左右摆</i></button>
<button data-action="reset">复位<i>回到正中</i></button>
</div>
</section>
<footer>动作按 50°/s 缓动执行，约 2–4 秒；期间按一下触摸会立即中止并回中</footer>
<div class="toast" id="toast"></div>
<script>
var names={normal:'正常',happy:'开心',blink:'眨眼',nod:'点头',shake:'摇头',reset:'复位'};
var toast=document.getElementById('toast'),timer=0;
function tip(msg){toast.textContent=msg;toast.classList.add('show');clearTimeout(timer);timer=setTimeout(function(){toast.classList.remove('show')},1500)}
function send(btn,url,label){
  btn.classList.add('done');
  fetch(url,{cache:'no-store'}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok)throw new Error(j.error||'busy');
    tip(label+' 已执行');
  }).catch(function(e){
    tip(label+' 失败：'+e.message);
  }).then(function(){
    setTimeout(function(){btn.classList.remove('done')},450);
  });
}
document.querySelectorAll('[data-face]').forEach(function(b){
  b.addEventListener('click',function(){send(b,'/api/face?type='+b.dataset.face,names[b.dataset.face])});
});
document.querySelectorAll('[data-action]').forEach(function(b){
  b.addEventListener('click',function(){send(b,'/api/action?do='+b.dataset.action,names[b.dataset.action])});
});
</script>
</body>
</html>
)HTML";

void sendError(int code, const char *message)
{
    gServer.send(code, "application/json",
                 String("{\"ok\":false,\"error\":\"") + message + "\"}");
}

void handleIndex()
{
    gServer.send_P(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleFace()
{
    if (!gServer.hasArg("type")) {
        sendError(400, "missing type");
        return;
    }
    const String type = gServer.arg("type");
    if (type == "normal") {
        displaySetMood(Mood::Normal);
    } else if (type == "happy") {
        displaySetMood(Mood::Happy);
    } else if (type == "blink") {
        displayTriggerBlink();
    } else {
        sendError(400, "type must be normal|happy|blink");
        return;
    }
    gServer.send(200, "application/json",
                 String("{\"ok\":true,\"face\":\"") + type + "\"}");
}

void handleAction()
{
    if (!gServer.hasArg("do")) {
        sendError(400, "missing do");
        return;
    }
    const String what = gServer.arg("do");
    if (what == "nod") {
        gPendingAction = RobotAction::Nod;
    } else if (what == "shake") {
        gPendingAction = RobotAction::Shake;
    } else if (what == "reset") {
        gPendingAction = RobotAction::Reset;
    } else {
        sendError(400, "do must be nod|shake|reset");
        return;
    }
    gPendingValid = true;
    gServer.send(200, "application/json",
                 String("{\"ok\":true,\"action\":\"") + what + "\"}");
}

void handleNotFound()
{
    gServer.send(404, "text/plain; charset=utf-8", "404 Not Found");
}

} // namespace

void webInit()
{
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(kApSsid, kApPassword)) {
        Serial.println("[WEB] 热点启动失败，检查内存占用");
    }

    gServer.on("/", HTTP_GET, handleIndex);
    gServer.on("/api/face", HTTP_GET, handleFace);
    gServer.on("/api/action", HTTP_GET, handleAction);
    gServer.onNotFound(handleNotFound);
    gServer.begin();

    Serial.printf("[WEB] AP \"%s\"（密码 %s）控制页 http://%s/\n", kApSsid,
                  kApPassword, WiFi.softAPIP().toString().c_str());
}

void webTick()
{
    gServer.handleClient();
}

bool webTakeAction(RobotAction &out)
{
    if (!gPendingValid) {
        return false;
    }
    gPendingValid = false;
    out = gPendingAction;
    return true;
}
