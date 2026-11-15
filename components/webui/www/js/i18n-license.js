/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
Object.assign(WEB_I18N_BUILTIN.en, {
  "license.title": "License", "license.device": "Device ID", "license.copy": "Copy device ID",
  "license.hint": "License status is informational. All panel features remain available, including after the trial ends.",
  "license.request": "Send this device ID to your supplier to request a signed DEVICE_KEY file.",
  "license.file": "Signed license file", "license.import": "Import license", "license.remove": "Remove license",
  "license.confirm_remove": "Remove the stored license? Panel features will remain available.",
  "license.valid": "Licensed permanently", "license.missing": "No license imported", "license.invalid": "Stored license is invalid",
  "license.error": "License storage or verification error", "license.unconfigured": "Issuer public key is not configured in this firmware",
  "license.trial": "Trial: {minutes} minutes remaining this boot", "license.expired": "Trial ended; all features remain available",
  "license.invalid_file": "This file is invalid or belongs to another device or issuer.",
  "license.unavailable": "Could not read or save the license. Refresh to check the current status.",
  "license.copied": "Device ID copied", "license.copy_failed": "Select the device ID and copy it manually.",
  "license.saved": "License saved", "license.removed": "License removed", "license.admin": "An administrator can import or remove a license.",
  "license.close": "Close", "license.refresh": "Refresh", "license.local": "Local preview; no device license is read or changed."
});
Object.assign(WEB_I18N_BUILTIN.de, {
  "license.title": "Lizenz", "license.device": "Geräte-ID", "license.copy": "Geräte-ID kopieren",
  "license.hint": "Der Lizenzstatus dient nur zur Information. Alle Funktionen bleiben verfügbar, auch nach Ende der Testzeit.",
  "license.request": "Senden Sie diese Geräte-ID an Ihren Anbieter, um eine signierte DEVICE_KEY-Datei anzufordern.",
  "license.file": "Signierte Lizenzdatei", "license.import": "Lizenz importieren", "license.remove": "Lizenz entfernen",
  "license.confirm_remove": "Gespeicherte Lizenz entfernen? Alle Funktionen bleiben verfügbar.",
  "license.valid": "Dauerhaft lizenziert", "license.missing": "Keine Lizenz importiert", "license.invalid": "Gespeicherte Lizenz ungültig",
  "license.error": "Fehler beim Speichern oder Prüfen der Lizenz", "license.unconfigured": "In dieser Firmware ist kein Anbieter-Public-Key eingerichtet",
  "license.trial": "Testzeit: noch {minutes} Minuten seit diesem Start", "license.expired": "Testzeit beendet; alle Funktionen bleiben verfügbar",
  "license.invalid_file": "Diese Datei ist ungültig oder gehört zu einem anderen Gerät oder Anbieter.",
  "license.unavailable": "Lizenz konnte nicht gelesen oder gespeichert werden. Aktualisieren Sie den Status.",
  "license.copied": "Geräte-ID kopiert", "license.copy_failed": "Markieren und kopieren Sie die Geräte-ID manuell.",
  "license.saved": "Lizenz gespeichert", "license.removed": "Lizenz entfernt", "license.admin": "Administratoren können Lizenzen importieren oder entfernen.",
  "license.close": "Schließen", "license.refresh": "Aktualisieren", "license.local": "Lokale Vorschau; keine Gerätelizenz wird gelesen oder geändert."
});
Object.assign(WEB_I18N_BUILTIN["zh-cn"], {
  "license.title": "授权管理", "license.device": "设备编号", "license.copy": "复制设备编号",
  "license.hint": "授权状态仅供查看。试用结束后，面板所有功能仍可正常使用。",
  "license.request": "请将设备编号提供给供应商，获取已签名的 DEVICE_KEY 授权文件。",
  "license.file": "签名授权文件", "license.import": "导入授权", "license.remove": "移除授权",
  "license.confirm_remove": "移除已保存的授权？面板功能不受影响。",
  "license.valid": "永久授权", "license.missing": "尚未导入授权", "license.invalid": "已保存的授权无效",
  "license.error": "授权存储或校验失败", "license.unconfigured": "当前固件尚未配置供应商公钥",
  "license.trial": "本次开机试用剩余 {minutes} 分钟", "license.expired": "试用已结束，所有功能仍可使用",
  "license.invalid_file": "文件无效，或不属于本设备或供应商。", "license.unavailable": "无法读取或保存授权，请刷新查看当前状态。",
  "license.copied": "设备编号已复制", "license.copy_failed": "请选中设备编号手动复制。",
  "license.saved": "授权已保存", "license.removed": "授权已移除", "license.admin": "管理员可以导入或移除授权。",
  "license.close": "关闭", "license.refresh": "刷新", "license.local": "本地预览，不读取或修改设备授权。"
});
// Additional catalogs use explicit English fallback until translations are supplied.
for (const catalog of Object.values(WEB_I18N_BUILTIN)) {
  for (const [key, value] of Object.entries(WEB_I18N_BUILTIN.en)) {
    if (key.startsWith("license.") && !(key in catalog)) catalog[key] = value;
  }
}
