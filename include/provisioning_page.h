#pragma once

const char PROVISIONING_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Настройка SmartBackpack</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:480px;margin:8vh auto;padding:0 20px;color:#17202a}
    h1{margin:0 0 8px}
    p{margin:0}
    .wifi-note{display:block;margin-top:6px;color:#687780;font-size:12px}
    .scan-bar{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:22px}
    .scan-status{color:#52616b;font-size:14px}
    .scan-button{margin:0;padding:9px 14px;border:1px solid #1769aa;border-radius:6px;background:#fff;color:#1769aa;font:inherit;font-size:14px;cursor:pointer;white-space:nowrap}
    .scan-button:disabled{opacity:.6;cursor:wait}
    .network-list{margin-top:10px;border:1px solid #d5dde2;border-radius:8px;max-height:260px;overflow-y:auto}
    .network-list[hidden]{display:none}
    .network-item{display:flex;align-items:center;gap:12px;width:100%;margin:0;padding:12px 14px;border:0;border-radius:0;border-bottom:1px solid #eef2f4;background:#fff;color:inherit;font:inherit;text-align:left;cursor:pointer}
    .network-item:last-child{border-bottom:0}
    .network-item:hover{background:#f3f8fb}
    .network-item[aria-pressed="true"]{background:#e5f1fa;box-shadow:inset 3px 0 #1769aa}
    .network-name{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .network-meta{display:flex;align-items:center;gap:6px;color:#687780;font-size:12px;white-space:nowrap}
    .bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
    .bars i{display:block;width:3px;border-radius:1px;background:#c3ced4}
    .bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:8px}.bars i:nth-child(3){height:11px}.bars i:nth-child(4){height:14px}
    .bars i.on{background:#1769aa}
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
  <p>Выберите сеть из списка или введите имя вручную.</p>
  <small class="wifi-note">Поддерживаются только сети Wi-Fi 2,4 ГГц.</small>
  <div class="scan-bar">
    <span class="scan-status" id="scan-status" role="status">Ищем сети…</span>
    <button class="scan-button" id="scan-button" type="button">Сканировать</button>
  </div>
  <div class="network-list" id="network-list" hidden></div>
  <form id="wifi-form" method="post" action="/save">
    <label for="ssid">Имя сети (SSID)</label>
    <input id="ssid" name="ssid" maxlength="32" required autocomplete="off" placeholder="Выберите сеть или введите имя">
    <label for="password">Пароль</label>
    <div class="password-field">
      <input id="password" name="password" type="password" maxlength="63" autocomplete="new-password">
      <button class="password-toggle" type="button" aria-label="Показать пароль" aria-pressed="false">
        <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M2 12s3.6-7 10-7 10 7 10 7-3.6 7-10 7S2 12 2 12z"></path><circle cx="12" cy="12" r="3"></circle><path class="slash" d="M3 3l18 18"></path></svg>
      </button>
    </div>
    <button id="submit-button" class="submit-button" type="submit"><span class="spinner" aria-hidden="true"></span><span class="submit-label">Сохранить и подключиться</span></button>
  </form>
  <script>
    const statusLabel = document.getElementById('scan-status');
    const scanButton = document.getElementById('scan-button');
    const networkList = document.getElementById('network-list');
    const ssidField = document.getElementById('ssid');
    const passwordField = document.getElementById('password');
    const passwordToggle = document.querySelector('.password-toggle');
    const form = document.getElementById('wifi-form');
    const submitButton = document.getElementById('submit-button');

    function setStatus(text) {
      statusLabel.textContent = text;
    }

    function countLabel(count) {
      const mod100 = count % 100;
      const mod10 = count % 10;
      let word = 'сетей';
      if (mod100 < 11 || mod100 > 14) {
        if (mod10 === 1) {
          word = 'сеть';
        } else if (mod10 >= 2 && mod10 <= 4) {
          word = 'сети';
        }
      }
      return count + ' ' + word;
    }

    function makeSignalBars(rssi) {
      const level = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
      const bars = document.createElement('span');
      bars.className = 'bars';
      bars.title = 'Уровень сигнала: ' + rssi + ' дБм';
      for (let i = 0; i < 4; i++) {
        const bar = document.createElement('i');
        if (i < level) {
          bar.className = 'on';
        }
        bars.appendChild(bar);
      }
      return bars;
    }

    function updateSelection() {
      const selected = ssidField.value;
      networkList.querySelectorAll('.network-item').forEach(function(item) {
        item.setAttribute('aria-pressed', item.dataset.ssid === selected ? 'true' : 'false');
      });
    }

    function selectNetwork(ssid) {
      ssidField.value = ssid;
      updateSelection();
      setStatus('Выбрана сеть: ' + ssid);
      passwordField.focus();
    }

    function renderNetworks(networks) {
      networkList.innerHTML = '';
      if (!networks.length) {
        networkList.hidden = true;
        setStatus('Сети не найдены. Попробуйте сканировать ещё раз.');
        return;
      }
      networks.forEach(function(network) {
        const item = document.createElement('button');
        item.type = 'button';
        item.className = 'network-item';
        item.dataset.ssid = network.ssid;
        item.setAttribute('aria-pressed', 'false');

        const name = document.createElement('span');
        name.className = 'network-name';
        name.textContent = network.ssid;

        const meta = document.createElement('span');
        meta.className = 'network-meta';
        meta.appendChild(makeSignalBars(network.rssi));
        if (network.auth !== 0) {
          const lock = document.createElement('span');
          lock.className = 'lock';
          lock.textContent = '🔒';
          lock.title = 'Защищена паролем';
          meta.appendChild(lock);
        }

        item.appendChild(name);
        item.appendChild(meta);
        item.addEventListener('click', function() {
          selectNetwork(network.ssid);
        });
        networkList.appendChild(item);
      });
      networkList.hidden = false;
      updateSelection();
      const selected = ssidField.value;
      if (selected && networks.some(function(network) { return network.ssid === selected; })) {
        setStatus('Выбрана сеть: ' + selected);
      } else {
        setStatus('Найдено ' + countLabel(networks.length) + '. Выберите сеть из списка.');
      }
    }

    async function scanNetworks() {
      scanButton.disabled = true;
      setStatus('Сканируем сети… это может занять несколько секунд.');
      try {
        const response = await fetch('/scan', { cache: 'no-store' });
        if (!response.ok) {
          throw new Error('HTTP ' + response.status);
        }
        const data = await response.json();
        renderNetworks(Array.isArray(data.networks) ? data.networks : []);
      } catch (error) {
        networkList.hidden = true;
        setStatus('Не удалось получить список сетей. Попробуйте ещё раз.');
      } finally {
        scanButton.disabled = false;
      }
    }

    passwordToggle.addEventListener('click', function() {
      const visible = passwordField.type === 'password';
      passwordField.type = visible ? 'text' : 'password';
      passwordToggle.setAttribute('aria-pressed', visible ? 'true' : 'false');
      passwordToggle.setAttribute('aria-label', visible ? 'Скрыть пароль' : 'Показать пароль');
    });

    form.addEventListener('submit', function() {
      submitButton.disabled = true;
      submitButton.setAttribute('aria-busy', 'true');
      submitButton.querySelector('.submit-label').textContent = 'Подключаемся...';
    });

    ssidField.addEventListener('input', updateSelection);
    scanButton.addEventListener('click', scanNetworks);
    scanNetworks();
  </script>
</body>
</html>
)rawliteral";
