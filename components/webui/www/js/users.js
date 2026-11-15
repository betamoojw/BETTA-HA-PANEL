/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 * Browser sessions stay in HttpOnly cookies; no credentials enter Web Storage or backups.
 */
let userAccess = { enabled: false, authenticated: false, role: "none", username: "", local: false };
let usersRows = [];
let usersEditing = null;
let userFetchOriginal = null;
let userSnapshotUrl = null;
let userBarObserver = null;

function userCanEdit() { return ["installer", "administrator"].includes(userAccess.role); }
function userSettingsSections() {
  return userAccess.role === "installer" ? SETTINGS_NAV_ITEMS.filter(item => item.sectionId === "settingsThemeSection") : SETTINGS_NAV_ITEMS;
}
function userIsAdmin() { return userAccess.role === "administrator"; }
function userElement(id) { return document.getElementById(id); }
function userTranslate() {
  for (const node of document.querySelectorAll("[data-user-i18n]")) node.textContent = t(node.dataset.userI18n);
  const label = userElement("userIdentity");
  if (label) label.textContent = userAccess.enabled
    ? `${userAccess.username} · ${t(`users.role.${userAccess.role}`)}` : t("users.open");
}

function userShowLogin(message = "") {
  userAccess.authenticated = false;
  userElement("userLogin").hidden = false;
  userElement("userAccountBar").hidden = true;
  userElement("userViewer").hidden = true;
  userElement("editorShell").classList.add("hidden");
  userElement("provisioningRoot").classList.add("hidden");
  userElement("userLoginMessage").textContent = message;
  userElement("remoteDisconnect")?.click();
  if (userSnapshotUrl) URL.revokeObjectURL(userSnapshotUrl);
  userSnapshotUrl = null;
  userElement("userSnapshot").removeAttribute("src");
  userElement("userPasswordDialog")?.close();
  userElement("userLoginName").focus();
}

function userInstallFetch() {
  if (userFetchOriginal) return;
  userFetchOriginal = window.fetch.bind(window);
  window.fetch = async (input, options = {}) => {
    const target = new URL(input instanceof Request ? input.url : input, window.location.href);
    if (target.origin !== window.location.origin || !target.pathname.startsWith("/api/"))
      return userFetchOriginal(input, options);
    const headers = new Headers(options.headers || (typeof input !== "string" ? input.headers : undefined));
    const method = String(options.method || (typeof input !== "string" ? input.method : "GET") || "GET").toUpperCase();
    if (method !== "GET" && method !== "HEAD") headers.set("X-Betta-Request", "1");
    const response = await userFetchOriginal(input, { ...options, headers, credentials: "same-origin" });
    if (response.status === 401 && userAccess.authenticated && !target.pathname.startsWith("/api/auth/")) {
      userAccess.authenticated = false;
      userShowLogin(t("users.expired"));
    }
    return response;
  };
}

async function userRequest(path, payload) {
  const response = await fetch(path, payload === undefined ? {} : {
    method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(payload),
  });
  const data = await response.json();
  if (!response.ok) {
    const key = response.status === 401 ? "users.invalid_login" : response.status === 403 ? "users.denied"
      : response.status === 429 ? "users.throttled" : response.status === 400 ? "users.invalid_change" : "users.unavailable";
    throw new Error(t(key));
  }
  return data;
}

function usersBuildChange(action, form, existing = null) {
  const username = String(form.username || "");
  const password = String(form.password || "");
  if (!/^[A-Za-z0-9_.-]{3,32}$/.test(username)) throw new Error(t("users.invalid_name"));
  if (action === "delete") return { action, username };
  if (!["create", "update"].includes(action)) throw new Error(t("users.invalid_change"));
  const length = new TextEncoder().encode(password).length;
  if ((action === "create" || length > 0) && (length < 8 || length > 64)) throw new Error(t("users.invalid_password"));
  if (password !== String(form.repeat || "")) throw new Error(t("users.mismatch"));
  if (!["viewer", "operator", "installer", "administrator"].includes(form.role)) throw new Error(t("users.invalid_change"));
  if (existing && existing.username !== username) throw new Error(t("users.invalid_name"));
  return { action, username, password, role: form.role, enabled: !!form.enabled };
}

function usersEdit(row = null) {
  usersEditing = row ? { ...row } : null;
  userElement("usersName").value = row?.username || "";
  userElement("usersName").disabled = !!row;
  userElement("usersPassword").value = "";
  userElement("usersRepeat").value = "";
  userElement("usersRole").value = row?.role || (usersRows.length ? "viewer" : "administrator");
  userElement("usersEnabled").checked = row?.enabled ?? true;
  userElement("usersFormTitle").textContent = t(row ? "users.edit" : "users.add");
  userElement("usersForm").hidden = false;
  userElement("usersMessage").textContent = "";
  (row ? userElement("usersPassword") : userElement("usersName")).focus();
}

function usersRender() {
  const body = userElement("usersTableBody");
  if (!body) return;
  body.replaceChildren();
  for (const row of usersRows) {
    const tr = document.createElement("tr");
    for (const value of [row.username, t(`users.role.${row.role}`), t(row.enabled ? "users.enabled" : "users.disabled")]) {
      const td = document.createElement("td"); td.textContent = value; tr.appendChild(td);
    }
    const actions = document.createElement("td");
    for (const [label, handler] of [["users.edit", () => usersEdit(row)], ["users.delete", async () => {
      if (!window.confirm(t("users.confirm_delete", { name: row.username }))) return;
      await usersSave(usersBuildChange("delete", row));
    }]]) {
      const button = document.createElement("button");
      button.type = "button"; button.className = "btn small"; button.textContent = t(label);
      button.setAttribute("aria-label", `${t(label)} ${row.username}`);
      button.onclick = handler; actions.appendChild(button);
    }
    tr.appendChild(actions); body.appendChild(tr);
  }
  userElement("usersEmpty").hidden = usersRows.length > 0;
  userElement("usersAdd").disabled = usersRows.length >= 16 || userAccess.local;
}

async function usersLoad() {
  if (!userIsAdmin()) return;
  if (userAccess.local) {
    usersRender(); userElement("usersEmpty").hidden = true;
    userElement("usersMessage").textContent = t("users.simulation"); return;
  }
  try {
    const result = await userRequest("/api/users");
    usersRows = result.users;
    usersRender();
  } catch (error) { userElement("usersMessage").textContent = error.message; }
}

async function usersSave(payload) {
  const controls = userElement("settingsUsersSection").querySelectorAll("button");
  controls.forEach(button => { button.disabled = true; });
  try {
    await userRequest("/api/users", payload);
    userElement("usersPassword").value = userElement("usersRepeat").value = "";
    userElement("usersForm").hidden = true;
    userAccess.authenticated = false;
    userShowLogin(t("users.saved"));
  } catch (error) { userElement("usersMessage").textContent = error.message; }
  finally { controls.forEach(button => { button.disabled = false; }); }
}

function usersBind() {
  userElement("usersAdd").onclick = () => usersEdit();
  userElement("usersRefresh").onclick = usersLoad;
  userElement("usersCancel").onclick = () => {
    userElement("usersForm").hidden = true;
    userElement("usersPassword").value = userElement("usersRepeat").value = "";
  };
  userElement("usersForm").onsubmit = async event => {
    event.preventDefault();
    try {
      const payload = usersBuildChange(usersEditing ? "update" : "create", {
        username: userElement("usersName").value, password: userElement("usersPassword").value,
        repeat: userElement("usersRepeat").value, role: userElement("usersRole").value, enabled: userElement("usersEnabled").checked,
      }, usersEditing);
      await usersSave(payload);
    } catch (error) { userElement("usersMessage").textContent = error.message; }
  };
  userElement("usersOpen").onclick = () => {
    setActivePane("settings"); setActiveSettingsSection("settingsUsersSection"); void usersLoad();
  };
  userElement("userLogout").onclick = async () => {
    try { await userRequest("/api/auth/logout", {}); window.location.reload(); }
    catch (error) { userElement("userBarMessage").textContent = error.message; }
  };
  userElement("userPasswordOpen").onclick = () => {
    userElement("userPasswordForm").reset();
    userElement("userPasswordMessage").textContent = "";
    userElement("userPasswordDialog").showModal();
    userElement("userOldPassword").focus();
  };
  userElement("userPasswordCancel").onclick = () => {
    userElement("userPasswordForm").reset(); userElement("userPasswordDialog").close();
  };
  userElement("userPasswordForm").onsubmit = async event => {
    event.preventDefault();
    const button = userElement("userPasswordSave"); button.disabled = true;
    try {
      const change = usersBuildChange("update", { username: userAccess.username, role: userAccess.role, enabled: true,
        password: userElement("userNewPassword").value, repeat: userElement("userRepeatPassword").value });
      if (!change.password) throw new Error(t("users.invalid_password"));
      await userRequest("/api/auth/password", { current_password: userElement("userOldPassword").value, new_password: change.password });
      userElement("userPasswordForm").reset();
      userElement("userPasswordDialog").close();
      userShowLogin(t("users.saved"));
    } catch (error) { userElement("userPasswordMessage").textContent = error.message; }
    finally { button.disabled = false; }
  };
}

async function userRefreshSnapshot() {
  const button = userElement("userRefreshSnapshot"); button.disabled = true;
  const abort = new AbortController(), timer = window.setTimeout(() => abort.abort(), 15000);
  try {
    let response = await fetch("/api/screenshot.jpg", { signal: abort.signal });
    if (response.status === 501) response = await fetch("/api/screenshot.png", { signal: abort.signal });
    if (!response.ok) throw new Error(t("users.unavailable"));
    const blob = await response.blob();
    if (!userAccess.authenticated) return;
    if (userSnapshotUrl) URL.revokeObjectURL(userSnapshotUrl);
    userSnapshotUrl = URL.createObjectURL(blob);
    userElement("userSnapshot").src = userSnapshotUrl;
    userElement("userViewerMessage").textContent = "";
  } catch (error) { userElement("userViewerMessage").textContent = error.message; }
  finally { window.clearTimeout(timer); button.disabled = false; }
}

async function userStartViewer() {
  userElement("editorShell").classList.add("hidden");
  userElement("userViewer").hidden = false;
  userElement("userNavigateRow").hidden = userAccess.role !== "operator";
  userElement("userRefreshSnapshot").onclick = userRefreshSnapshot;
  if (userAccess.role === "operator") {
    const layout = await userRequest("/api/layout");
    const select = userElement("userPage"); select.replaceChildren();
    for (const page of layout.pages || []) {
      if (page.visible === false) continue;
      const option = document.createElement("option"); option.value = page.id;
      option.textContent = page.name || page.id; select.appendChild(option);
    }
    userElement("userNavigate").onclick = async () => {
      try { await userRequest("/api/navigate", { page: select.value }); await userRefreshSnapshot(); }
      catch (error) { userElement("userViewerMessage").textContent = error.message; }
    };
  }
  await userRefreshSnapshot();
}

async function userBootstrap() {
  const lang = String(navigator.language || "en").toLowerCase();
  const code = lang.startsWith("zh") ? (/(tw|hk|hant)/.test(lang) ? "zh-tw" : "zh-cn") : lang.split("-")[0];
  editor.i18nMap = { ...WEB_I18N_BUILTIN.en, ...(WEB_I18N_BUILTIN[code] || {}) };
  userTranslate(); userInstallFetch(); usersBind();
  userElement("userLoginForm").onsubmit = async event => {
    event.preventDefault(); userElement("userLoginSubmit").disabled = true;
    try {
      await userRequest("/api/auth/login", { username: userElement("userLoginName").value, password: userElement("userLoginPassword").value });
      userElement("userLoginPassword").value = ""; window.location.reload();
    } catch (error) { userElement("userLoginMessage").textContent = error.message; }
    finally { userElement("userLoginPassword").value = ""; userElement("userLoginSubmit").disabled = false; }
  };
  try {
    // The local fixture server injects this marker; device builds never do.
    if (document.body.dataset.localUi) userAccess = { enabled: false, authenticated: true, role: "administrator", username: "", local: true };
    else userAccess = await userRequest("/api/auth/session");
    if (!userAccess.authenticated) { userShowLogin(); return; }
    userElement("userLogin").hidden = true;
    userElement("userAccountBar").hidden = userAccess.local;
    userElement("usersOpen").hidden = !userIsAdmin();
    userElement("userLogout").hidden = !userAccess.enabled;
    userElement("userPasswordOpen").hidden = !userAccess.enabled;
    document.body.dataset.userRole = userAccess.role;
    if (typeof ResizeObserver === "function") {
      const bar = userElement("userAccountBar");
      userBarObserver = new ResizeObserver(() => document.documentElement.style.setProperty("--user-bar-height", `${bar.getBoundingClientRect().height}px`));
      userBarObserver.observe(bar);
    }
    userTranslate();
    if (userCanEdit()) {
      await bootstrap();
      if (!userAccess.authenticated) { userShowLogin(t("users.expired")); return; }
      if (!userIsAdmin()) {
        userElement("settingsUiTextStep").disabled = true;
        userElement("settingsAppearanceHeading").nextElementSibling.querySelectorAll("input, select, button").forEach(node => { node.disabled = true; });
      }
      userTranslate();
    } else {
      const settings = await userRequest("/api/settings");
      if (settings.ui?.language) await loadI18nLanguage(settings.ui.language, true);
      userTranslate(); await userStartViewer();
    }
    if (!userAccess.local) window.setInterval(async () => {
      try {
        const session = await userRequest("/api/auth/session");
        if (!session.authenticated && userAccess.authenticated) { userAccess.authenticated = false; userShowLogin(t("users.expired")); }
      } catch (_) { /* Network errors are not authentication results. */ }
    }, 60000);
  } catch (error) { userShowLogin(error.message); }
}
