/* SPDX-License-Identifier: LicenseRef-FNCL-1.1
 * Copyright (c) 2026 andre68723
 */
function pageLimitForWidth(width) {
  const policy = width >= 1000 ? 12 : width >= 700 ? 8 : 5;
  // nav_layout_max_pages(): margins 14, compact tab 64, gap 8, selected tab width/4.
  const fit = Math.max(1, Math.min(16, 1 + Math.floor((width - 28 - Math.floor(width / 4)) / 72)));
  return Math.min(policy, fit);
}

function maxWidgetsPerPage() {
  return editor.appMaxWidgetsPerPage || MAX_WIDGETS_PER_PAGE;
}

function maxWidgetsTotal() {
  return Math.min(editor.appMaxWidgetsTotal || 224, maxPages() * maxWidgetsPerPage());
}

function layoutCapacity(layout = editor.layout, page = selectedPage()) {
  const pages = Array.isArray(layout?.pages) ? layout.pages : [];
  const widgets = pages.reduce((sum, item) => sum + (Array.isArray(item.widgets) ? item.widgets.length : 0), 0);
  const pageWidgets = Array.isArray(page?.widgets) ? page.widgets.length : 0;
  const remainingTotal = Math.max(0, maxWidgetsTotal() - widgets);
  return {
    pages: pages.length, widgets, pageWidgets,
    remainingPages: Math.max(0, maxPages() - pages.length), remainingTotal,
    remainingPage: page && !isFixedPage(page) ? Math.max(0, Math.min(maxWidgetsPerPage() - pageWidgets, remainingTotal)) : null,
  };
}

function layoutOrientationStale() {
  return !!editor.layout?.orientation && editor.appScreenW > 0 && editor.appScreenW !== CANVAS_WIDTH;
}

function requireLayoutCapacity({ pages = 0, widgets = 0, page = null } = {}) {
  const usage = layoutCapacity();
  let message = "";
  if (layoutOrientationStale()) message = t("layout.capacity.rotated");
  else if (pages && usage.pages + pages > maxPages()) message = t("layout.status.max_pages", { max: maxPages() });
  else if (widgets && page && (page.widgets?.length || 0) + widgets > maxWidgetsPerPage()) {
    message = t("layout.status.max_widgets_page", { max: maxWidgetsPerPage() });
  } else if (widgets && usage.widgets + widgets > maxWidgetsTotal()) {
    message = t("layout.status.max_widgets_total", { max: maxWidgetsTotal() });
  }
  if (message) setStatus(message, true);
  return !message;
}

function renderLayoutCapacity() {
  const bar = document.getElementById("layoutCapacity");
  if (!bar) return;
  const usage = layoutCapacity();
  for (const [id, key, remaining, used, max] of [
    ["layoutCapacityPages", "pages", usage.remainingPages, usage.pages, maxPages()],
    ["layoutCapacityPage", "page", usage.remainingPage, usage.pageWidgets, maxWidgetsPerPage()],
    ["layoutCapacityTotal", "total", usage.remainingTotal, usage.widgets, maxWidgetsTotal()],
  ]) {
    const node = document.getElementById(id);
    if (!node) continue;
    const text = remaining === null ? t("layout.capacity.fixed") : t(`layout.capacity.${key}`, { remaining, used, max });
    if (node.textContent !== text) node.textContent = text;
    node.classList.toggle("capacity-full", remaining === 0);
    node.classList.toggle("capacity-over", remaining !== null && used > max);
  }
  const hint = document.getElementById("layoutCapacityHint");
  if (hint) {
    const text = layoutOrientationStale() ? t("layout.capacity.rotated") : usage.pages > maxPages()
      ? t("layout.capacity.exceeded", { count: usage.pages - maxPages() }) : "";
    if (hint.textContent !== text) hint.textContent = text;
    hint.hidden = !text;
  }
  bar.title = t("layout.capacity.help");
}

function applyLayoutLimits(payload) {
  if (!payload || typeof payload !== "object") return;
  for (const [key, field] of [["screen_w", "appScreenW"], ["screen_h", "appScreenH"],
    ["max_pages", "appMaxPages"], ["max_pages_capacity", "appMaxPagesCapacity"],
    ["max_widgets_per_page", "appMaxWidgetsPerPage"], ["max_widgets_total", "appMaxWidgetsTotal"],
    ["full_nav_pages", "appFullNavPages"]]) {
    if (Number.isSafeInteger(payload[key]) && payload[key] > 0) editor[field] = payload[key];
  }
  renderLayoutCapacity();
}

let layoutLimitsRequest = null;
async function refreshLayoutLimits() {
  if (layoutLimitsRequest) return layoutLimitsRequest;
  layoutLimitsRequest = (async () => {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 4000);
    try { applyLayoutLimits(await apiGet("/api/layout?limits=1", controller.signal)); }
    catch (_) { /* Older firmware/offline: retain the last known limits; saving is still checked by the API. */ }
    finally { clearTimeout(timeout); layoutLimitsRequest = null; }
  })();
  return layoutLimitsRequest;
}
