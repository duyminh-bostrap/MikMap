# Hợp đồng license MikMap ↔ web (bản nháp cho phía web)

Tệp này là **đặc tả để chép sang repo web** `duyminh-bostrap/Mike-Portfolio`. Repo này không sửa gì ở web. Đây là bản nháp: phía web cần rà lại với schema thật (`products`, `profiles`, `private.require_admin()`, mẫu `add-free-keys.sql`, `add-mikedid-licenses.sql`) trước khi chạy.

## Nguyên tắc

- Entitlement **riêng cho MikMap** (ADR 0001). App MikMap không gọi `get_entitlement()` của MikMaster, và một key MikMap không bao giờ mở MikMaster hay MikEDID, ngược lại cũng vậy.
- Web quyết định ai được key nào, loại gì (dùng thử, 1 tháng, 1 năm, vĩnh viễn kèm cập nhật). App chỉ nhận kết quả.
- Một key chạy trên một máy. Kích hoạt và gỡ key cần online.
- App gọi bằng khoá anon công khai, không cần đăng nhập (giống `activate_free_key`). Key là bí mật ngẫu nhiên do máy chủ kiểm, không phải token ký.

## Key

Dạng `MIKP-XXXXX-XXXXX-XXXXX-XXXXX` (hex hoa). Người dùng lấy key ở trang `/store/mikmap/license` sau khi có quyền (mua hoặc dùng thử). Tối đa số key mỗi tài khoản và giới hạn tạo key: theo mẫu `free_keys` (10 key chưa thu hồi, 30 key mỗi 30 ngày), TODO(owner): xác nhận con số.

## RPC cho app

### `activate_mikmap_key(p_key text, p_machine text) -> json`

Role: `anon` và `authenticated`. Lần đầu gọi với key hợp lệ và chưa gắn máy thì **gắn** key vào máy đó. Gọi lại cùng máy thì cập nhật `last_seen_at` và trả trạng thái hiện tại.

```json
{ "state": "licensed", "expires_at": null, "updates_until": "2027-10-10T00:00:00+00:00", "server_now": "2026-10-10T09:00:00+00:00" }
```

| `state` | Nghĩa | App làm |
|---|---|---|
| `licensed` | Key trả phí hợp lệ cho máy này | Bỏ watermark, cho gửi sensor |
| `trial` | Dùng thử còn hạn | Như `licensed`, bỏ qua `updates_until` |
| `expired` | Hết hạn dùng (`expires_at` đã qua) | Chạy như không license |
| `other_machine` | Key đang gắn với máy khác. Kèm `machine_hint` (4 ký tự cuối mã máy) | Báo "key đang dùng ở máy khác", chỉ dẫn gỡ ở máy cũ hoặc trên web |
| `revoked` | Chủ tài khoản đã thu hồi | Báo đã bị thu hồi |
| `invalid` | Không phải key do web cấp | Báo key không đúng |

- `expires_at`: hết hạn **dùng**; `null` = không hết hạn. `updates_until`: bản build phát hành sau mốc này không được phủ; `null` = không giới hạn.
- `server_now` luôn có. App dùng nó (không dùng đồng hồ máy) để so hạn.
- Mã máy sai định dạng (`^[0-9A-F]{4}(-[0-9A-F]{4}){3}$`) trả lỗi `bad machine code` (HTTP 400).

### `release_mikmap_key(p_key text, p_machine text) -> json`

Gỡ key khỏi máy **đang giữ nó** (đúng cặp key + mã máy), để key kích hoạt được trên máy khác. Cần online.

```json
{ "released": true, "server_now": "…" }
```

Sai key hoặc sai máy trả `{ "released": false }` (không lộ thông tin gì thêm). Giới hạn: mỗi key gỡ tối đa 5 lần trong 30 ngày (như `release_my_device`), TODO(owner): xác nhận con số. Chủ web gỡ không giới hạn trong Dashboard.

## Phía web (người dùng và quản trị)

- Trang `/store/mikmap/license`: tạo key, xem key, gỡ máy đang giữ key, thu hồi key (mẫu `FreeKeysCard`).
- Dashboard → Licenses: cấp key kiểu **dùng thử / 1 tháng / 1 năm / vĩnh viễn + cập nhật N tháng** cho một tài khoản, và "Release computer".
- Đơn hàng MikMap chỉ ghi quyền MikMap, không động tới bảng entitlement của MikMaster.

## SQL tham khảo (cần rà lại trước khi chạy)

```sql
create table if not exists public.mikmap_keys (
  id            uuid primary key default gen_random_uuid(),
  user_id       uuid not null references auth.users (id) on delete cascade,
  key           text not null unique check (key ~ '^MIKP-[0-9A-F]{5}(-[0-9A-F]{5}){3}$'),
  kind          text not null check (kind in ('trial', 'paid')),
  expires_at    timestamptz,                 -- hết hạn dùng; null = không hết
  updates_until timestamptz,                 -- cập nhật đến; null = không giới hạn
  machine_code  text check (machine_code is null or machine_code ~ '^[0-9A-F]{4}(-[0-9A-F]{4}){3}$'),
  label         text check (label is null or length(label) <= 60),
  created_at    timestamptz not null default now(),
  activated_at  timestamptz,
  last_seen_at  timestamptz,
  revoked_at    timestamptz
);
create index if not exists mikmap_keys_user_idx on public.mikmap_keys (user_id, created_at desc);
alter table public.mikmap_keys enable row level security;
revoke all on public.mikmap_keys from anon, authenticated;
grant select on public.mikmap_keys to authenticated;   -- cộng một policy "đọc key của chính mình", như free_keys

create table if not exists public.mikmap_key_log (
  id           bigint generated always as identity primary key,
  key_id       uuid not null references public.mikmap_keys (id) on delete cascade,
  machine_code text not null,
  action       text not null check (action in ('bind', 'release', 'admin_release')),
  at           timestamptz not null default now()
);
alter table public.mikmap_key_log enable row level security;
revoke all on public.mikmap_key_log from anon, authenticated;

create or replace function public.activate_mikmap_key(p_key text, p_machine text)
returns json language plpgsql security definer set search_path = '' as $$
declare
  machine text := upper(btrim(coalesce(p_machine, '')));
  k public.mikmap_keys;
begin
  if machine !~ '^[0-9A-F]{4}(-[0-9A-F]{4}){3}$' then
    raise exception 'bad machine code' using errcode = '22023';
  end if;
  select * into k from public.mikmap_keys where key = upper(btrim(coalesce(p_key, ''))) for update;
  if not found then return json_build_object('state', 'invalid', 'server_now', now()); end if;
  if k.revoked_at is not null then return json_build_object('state', 'revoked', 'server_now', now()); end if;

  if k.machine_code is null then
    update public.mikmap_keys set machine_code = machine, activated_at = now(), last_seen_at = now() where id = k.id;
    insert into public.mikmap_key_log (key_id, machine_code, action) values (k.id, machine, 'bind');
  elsif k.machine_code <> machine then
    return json_build_object('state', 'other_machine', 'machine_hint', right(k.machine_code, 4), 'server_now', now());
  else
    update public.mikmap_keys set last_seen_at = now() where id = k.id;
  end if;

  if k.expires_at is not null and k.expires_at <= now() then
    return json_build_object('state', 'expired', 'expires_at', k.expires_at, 'server_now', now());
  end if;
  return json_build_object(
    'state', case k.kind when 'trial' then 'trial' else 'licensed' end,
    'expires_at', k.expires_at, 'updates_until', k.updates_until, 'server_now', now());
end;
$$;

create or replace function public.release_mikmap_key(p_key text, p_machine text)
returns json language plpgsql security definer set search_path = '' as $$
declare
  machine text := upper(btrim(coalesce(p_machine, '')));
  k public.mikmap_keys;
  used int;
begin
  select * into k from public.mikmap_keys where key = upper(btrim(coalesce(p_key, ''))) for update;
  if not found or k.revoked_at is not null or k.machine_code is distinct from machine then
    return json_build_object('released', false, 'server_now', now());
  end if;
  select count(*) into used from public.mikmap_key_log
    where key_id = k.id and action = 'release' and at > now() - interval '30 days';
  if used >= 5 then raise exception 'rate limited' using errcode = '53400'; end if;
  update public.mikmap_keys set machine_code = null where id = k.id;
  insert into public.mikmap_key_log (key_id, machine_code, action) values (k.id, machine, 'release');
  return json_build_object('released', true, 'server_now', now());
end;
$$;

revoke all on function public.activate_mikmap_key(text, text) from public;
revoke all on function public.release_mikmap_key(text, text) from public;
grant execute on function public.activate_mikmap_key(text, text) to anon, authenticated;
grant execute on function public.release_mikmap_key(text, text) to anon, authenticated;
notify pgrst, 'reload schema';
```

Còn thiếu trong bản nháp (phía web làm, theo mẫu `free_keys` và `add-licenses.sql`): hàm tạo key cho người dùng có quyền (`create_mikmap_key`), hàm quản trị cấp/thu hồi, chính sách RLS đọc key của chính mình, và chống dò key bằng cách giới hạn tần suất gọi (key 20 hex có 80 bit nên khó đoán, nhưng RPC `anon` vẫn nên có giới hạn).

## Gọi từ app

`POST {SUPABASE_URL}/rest/v1/rpc/activate_mikmap_key` với header `apikey` (khoá anon) và body `{"p_key": "...", "p_machine": "XXXX-XXXX-XXXX-XXXX"}`. `release_mikmap_key` tương tự. Trong app chỉ có URL và khoá anon công khai; không bao giờ để khoá dịch vụ.
