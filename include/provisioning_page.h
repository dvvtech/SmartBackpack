#pragma once

const char PROVISIONING_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Настройка SmartBackpack</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:480px;margin:10vh auto;padding:0 20px;color:#17202a}
    .wifi-note{display:block;margin-top:6px;color:#687780;font-size:12px}
    label{display:block;margin:18px 0 6px}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #aab7b8;border-radius:6px;font:inherit}
    button{margin-top:22px;padding:12px 18px;border:0;border-radius:6px;background:#1769aa;color:white;font:inherit;cursor:pointer}
    .password-field{position:relative}.password-field input{padding-right:48px}
    .password-toggle{position:absolute;right:4px;top:50%;transform:translateY(-50%);margin:0;padding:8px;border:0;background:transparent;color:#52616b;line-height:0}
    .password-toggle svg{width:20px;height:20px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
    .password-toggle .slash{display:none}.password-toggle[aria-pressed="true"] .slash{display:block}
    .submit-button{display:inline-flex;align-items:center;gap:9px}.submit-button:disabled{opacity:.75;cursor:wait}
    .spinner{display:none;width:16px;height:16px;border:2px solid rgba(255,255,255,.45);border-top-color:#fff;border-radius:50%;animation:spin .7s linear infinite}
    .submit-button[aria-busy="true"] .spinner{display:inline-block}@keyframes spin{to{transform:rotate(360deg)}}
  </style>
</head>
<body>
  <h1>Настройка Wi-Fi</h1>
  <p>Укажите сеть, к которой должно подключаться устройство.</p>
  <small class="wifi-note">Поддерживаются только сети Wi-Fi 2,4 ГГц.</small>
  <form id="wifi-form" method="post" action="/save">
    <label for="ssid">Имя сети (SSID)</label>
    <input id="ssid" name="ssid" maxlength="32" required autocomplete="off">
    <label for="password">Пароль</label>
    <div class="password-field">
      <input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
      <button class="password-toggle" type="button" aria-label="Показать пароль" aria-pressed="false" onclick="togglePassword()">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z"></path><circle cx="12" cy="12" r="3"></circle><path class="slash" d="M3 3l18 18"></path></svg>
      </button>
    </div>
    <button id="submit-button" class="submit-button" type="submit"><span class="spinner" aria-hidden="true"></span><span class="submit-label">Сохранить и подключиться</span></button>
  </form>
  <script>
    function togglePassword(){
      const field=document.getElementById('password');
      const toggle=document.querySelector('.password-toggle');
      const visible=field.type==='password';
      field.type=visible?'text':'password';
      toggle.setAttribute('aria-pressed',visible?'true':'false');
      toggle.setAttribute('aria-label',visible?'Скрыть пароль':'Показать пароль');
    }
    document.getElementById('wifi-form').addEventListener('submit',function(){
      const button=document.getElementById('submit-button');
      button.disabled=true;
      button.setAttribute('aria-busy','true');
      button.querySelector('.submit-label').textContent='Подключаемся...';
    });
  </script>
</body>
</html>
)rawliteral";

