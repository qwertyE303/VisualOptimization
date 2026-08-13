#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>

#include "bml_includes.hpp"

extern "C" {
  __declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

// ============================================================
// VisualOptimization（视觉优化）v1.0.0
//   合并自 BallanceBug 的 ViewDistanceEditor（视距调整功能），
//   并新增两个功能：
//     1) Preload：进关黑屏期强制渲染全部场景网格（消除飞行卡顿）
//     2) 光照优化：深背光区 5 个微弱补光 + 主光按关卡设定值×0.985（背光面不黑且均匀）
//
//   编译宏（CMakeLists 里配置）：
//     CONFIG_MODE —— 可配置版：三个距离输入栏（默认 1200，游戏原版默认值）+ preload/光照优化 enabled 开关
//     (无宏)      —— 写死版：视距 1e9、preload 开、光照优化开，安装即用
// ============================================================

static constexpr float FIXED_VIEW_DISTANCE = 1e+9f;    // 写死版视距
static constexpr float FIXED_LIFE_DISTANCE  = 1e+9f;
static constexpr float FIXED_POINT_DISTANCE = 1e+9f;

static constexpr float DEFAULT_VIEW_DISTANCE = 1200.0f; // 配置版默认（游戏原版默认值）
static constexpr float DEFAULT_LIFE_DISTANCE  = 1200.0f;
static constexpr float DEFAULT_POINT_DISTANCE = 1200.0f;

// ---- 光照优化参数（v0.6 实测定稿） ----
static const int    kFillCount  = 5;
static const float  kBackAmbient = 0.15f;  // 暗部最大补光强度 A_back
static const float  kMainDim     = 0.985f; // 主光整体倍率：主光最终值 = 关卡脚本设定值 × 0.985
static const float  kFillCosLo   = -0.35f; // 补光深度下界 cosT（θ≈110.5°）
static const float  kFillCosHi   = -1.0f;  // 补光深度上界（正后方）
static const char* kFillNames[kFillCount] = {
  "VisualOpt_Fill0",
  "VisualOpt_Fill1",
  "VisualOpt_Fill2",
  "VisualOpt_Fill3",
  "VisualOpt_Fill4",
};

class VisualOptimization : public IMod {
  bool init = false, notify = true;

#ifdef CONFIG_MODE
  float view_distance{ DEFAULT_VIEW_DISTANCE };
  float life_distance{ DEFAULT_LIFE_DISTANCE };
  float point_distance{ DEFAULT_POINT_DISTANCE };
  bool  preload_enabled = true;
  bool  lighting_enabled = true;
  IProperty* prop_view_distance{}, * prop_life_distance{}, * prop_point_distance{};
  IProperty* prop_preload{}, * prop_lighting{};
#else
  static constexpr float view_distance   = FIXED_VIEW_DISTANCE;
  static constexpr float life_distance   = FIXED_LIFE_DISTANCE;
  static constexpr float point_distance  = FIXED_POINT_DISTANCE;
  static constexpr bool  preload_enabled = true;
  static constexpr bool  lighting_enabled = true;
#endif

  bool render_preloaded = false;   // 当前关已执行全场景网格预渲染
  bool lighting_applied = false;   // 当前关已应用光照优化
  int  lighting_frames  = 0;

  // 主光原始颜色/名字（用于退出时精确恢复；保留各关卡亮度特色，如 12 关主光较弱）
  VxColor main_original_color{1.0f, 1.0f, 1.0f};
  char    main_name[64] = "";
  bool    main_color_saved = false;
  float   last_main_intensity = 0.0f; // 已应用的主光强度（用于自适应跟随）

  // 每次使用时按名查找相机，避免关卡切换（上飞船/跳转小节）后缓存指针悬空
  auto get_ingame_cam() { return m_bml->GetTargetCameraByName("InGameCam"); }

  // ---------------- VDE：视距 / 生命球 / 分数球 ----------------
  void set_view_distance(float new_view_distance) {
    auto cam = get_ingame_cam();
    if (!cam) return;
    if (cam->GetBackPlane() == new_view_distance) return;
    cam->SetBackPlane(new_view_distance);
    if (!notify) return;
    char msg[48];
    std::snprintf(msg, sizeof(msg), "View distance set to %.5g", new_view_distance);
    m_bml->SendIngameMessage(msg);
    notify = false;
  }

  void apply_extra_life() {
    if (!init) return;
    auto* script = m_bml->GetScriptByName("P_Extra_Life_MF Script");
    if (!script) return;
    auto* bb = ScriptHelper::FindFirstBB(script, "TT Scaleable Proximity");
    if (!bb) return;
    float current = 0.0f;
    bb->GetInputParameter(0)->GetDirectSource()->GetValue(&current);
    if (current == life_distance) return;
    auto temp = life_distance;
    bb->GetInputParameter(0)->GetDirectSource()->SetValue(&temp);
    bb->GetInputParameter(4)->GetDirectSource()->SetValue(&temp);
    temp += 10;
    bb->GetInputParameter(5)->GetDirectSource()->SetValue(&temp);
  }

  void apply_extra_point() {
    if (!init) return;
    auto* script = m_bml->GetScriptByName("P_Extra_Point_MF Script");
    if (!script) return;
    auto* bb = ScriptHelper::FindFirstBB(script, "TT Scaleable Proximity");
    if (!bb) return;
    float current = 0.0f;
    bb->GetInputParameter(0)->GetDirectSource()->GetValue(&current);
    if (current == point_distance) return;
    auto temp = point_distance;
    bb->GetInputParameter(0)->GetDirectSource()->SetValue(&temp);
    temp += 5;
    bb->GetInputParameter(4)->GetDirectSource()->SetValue(&temp);
    temp += 15;
    bb->GetInputParameter(5)->GetDirectSource()->SetValue(&temp);
  }

  void apply_all() {
    if (!init) return;
    set_view_distance(view_distance);
    apply_extra_life();
    apply_extra_point();
  }

  // ---------------- Preload：全场景网格预渲染 ----------------
  static bool is_3d_entity(CKObject* obj) {
    if (!obj) return false;
    return CKIsChildClassOf(obj->GetClassID(), CKCID_3DENTITY);
  }

  void prerender_meshes(CKContext* ctx) {
    if (!ctx) return;
    CKRenderContext* rc = m_bml->GetRenderContext();
    if (!rc) return;
    std::set<CK3dEntity*> ents; // 去重，防同一实体被多个 class 列表重复处理
    auto add_from_list = [&](CK_CLASSID cid) {
      int cnt = ctx->GetObjectsCountByClassID(cid);
      CK_ID* ids = ctx->GetObjectsListByClassID(cid);
      if (!ids) return;
      for (int i = 0; i < cnt; i++) {
        CKObject* obj = ctx->GetObject(ids[i]);
        if (!obj || !is_3d_entity(obj)) continue;
        ents.insert(static_cast<CK3dEntity*>(obj));
      }
    };
    add_from_list(CKCID_3DENTITY);
    add_from_list(CKCID_3DOBJECT);
    int meshCount = 0;
    for (auto* ent : ents) {
      int mc = ent->GetMeshCount();
      for (int j = 0; j < mc; j++) {
        CKMesh* mesh = ent->GetMesh(j);
        if (!mesh) continue;
        mesh->Render(rc, ent);
        meshCount++;
      }
    }
    char msg[64];
    std::snprintf(msg, sizeof(msg), "Preloaded %d static meshes", meshCount);
    m_bml->SendIngameMessage(msg);
  }

  // ---------------- 光照优化：深背光补光 ----------------
  CKLight* findMainLight(CKContext* ctx) {
    const XObjectPointerArray& lights = ctx->GetObjectListByType(CKCID_LIGHT, TRUE);
    CKLight* fallback = NULL;
    for (int i = 0; i < lights.Size(); i++) {
      CKLight* light = (CKLight*)(lights[i]);
      if (!light || !light->GetActivity()) continue;
      if (light->GetType() != VX_LIGHTDIREC) continue;
      if (!fallback) fallback = light;
      const char* n = light->GetName();
      if (n && strstr(n, "Ingame")) return light;
    }
    return fallback;
  }

  void applyLighting() {
    auto* logger = GetLogger();
    CKContext* ctx = m_bml->GetCKContext();
    CKRenderContext* rc = m_bml->GetRenderContext();
    if (!ctx || !rc) { logger->Warn("[VisualOpt] ctx/rc null"); return; }
    CKScene* scene = ctx->GetCurrentScene();
    if (!scene) { logger->Warn("[VisualOpt] no current scene"); return; }

    logger->Info("========== VisualOptimization lighting apply ==========");

    // 1) 主光：读原始颜色/方向/功率，按比例 × kMainDim（-1%）
    CKLight* main = findMainLight(ctx);
    if (!main) { logger->Warn("[VisualOpt] no active directional light, skip fill lights"); return; }
    VxVector dir;
    main->GetOrientation(&dir, NULL, NULL);
    main_original_color = main->GetColor();
    main_color_saved = true;
    const char* mname = main->GetName();
    if (mname) std::snprintf(main_name, sizeof(main_name), "%s", mname); else main_name[0] = '\0';
    main->SetColor(VxColor(main_original_color.r * kMainDim,
                           main_original_color.g * kMainDim,
                           main_original_color.b * kMainDim));
    float mainPower = main->GetLightPower(); // Virtools 2.1 API（探针已验证可用）
    float mainIntensity = (main_original_color.r + main_original_color.g + main_original_color.b) / 3.0f * mainPower;
    if (mainIntensity <= 0.01f) mainIntensity = 1.0f; // 防止黑色/异常主光导致补光消失
    last_main_intensity = mainIntensity;
    logger->Info("[VisualOpt] main=\"%s\" colorBefore=(%.2f,%.2f,%.2f) colorAfter=(%.2f,%.2f,%.2f) power=%.3f intensity=%.3f dir=(%.4f,%.4f,%.4f)",
      main_name,
      main_original_color.r, main_original_color.g, main_original_color.b,
      main_original_color.r * kMainDim, main_original_color.g * kMainDim, main_original_color.b * kMainDim,
      mainPower, mainIntensity, dir.x, dir.y, dir.z);

    // 2) 计算深背光补光方向（等立体角 + 黄金角，基于主光朝向）
    VxVector Lm(-dir.x, -dir.y, -dir.z);
    Lm = Normalize(Lm);
    VxVector w = Lm;
    VxVector e2(0.0f, 0.0f, 1.0f);
    if (fabsf(w.Dot(e2)) > 0.99f) e2 = VxVector(0.0f, 1.0f, 0.0f);
    VxVector u = CrossProduct(w, e2);
    u = Normalize(u);
    VxVector v = CrossProduct(u, w);
    v = Normalize(v);

    // 补光强度按主光强度自适应（12 关等主光较弱的关卡，补光同步减弱，保持相对亮度一致）
    float fillStrength = 2.0f * kBackAmbient * mainIntensity / (float)kFillCount;

    // 清理残留（防止 Restart/重复触发造成对象堆积）
    for (int i = 0; i < kFillCount; i++) {
      CKObject* stale = ctx->GetObjectByName((char*)kFillNames[i]);
      if (stale) ctx->DestroyObject(stale);
    }

    // 3) 创建补光
    for (int i = 0; i < kFillCount; i++) {
      float cosT = kFillCosLo + (kFillCosHi - kFillCosLo) * (i + 0.5f) / (float)kFillCount;
      float sinT = sqrtf(1.0f - cosT * cosT);
      float phi  = 2.399963f * (float)i;
      float sx = sinT * cosf(phi), sy = sinT * sinf(phi), sz = cosT;
      VxVector srcDir(
        u.x * sx + v.x * sy + w.x * sz,
        u.y * sx + v.y * sy + w.y * sz,
        u.z * sx + v.z * sy + w.z * sz);
      VxVector lightDir(-srcDir.x, -srcDir.y, -srcDir.z);

      CKLight* fill = (CKLight*)(ctx->CreateObject(CKCID_LIGHT, (char*)kFillNames[i]));
      if (!fill) { logger->Warn("[VisualOpt] FAILED to create fill[%d]", i); continue; }

      VxVector upA(0.0f, 1.0f, 0.0f);
      VxVector rightA = CrossProduct(lightDir, upA);
      if (rightA.Dot(rightA) < 1e-8f) {
        upA = VxVector(0.0f, 0.0f, 1.0f);
        rightA = CrossProduct(lightDir, upA);
      }
      rightA = Normalize(rightA);
      upA = CrossProduct(rightA, lightDir);
      upA = Normalize(upA);

      fill->SetType(VX_LIGHTDIREC);
      fill->SetColor(VxColor(fillStrength, fillStrength, fillStrength));
#ifdef BML_ORIENTATION_REF //老BML v0.3.30+：SetOrientation 参数1/2 为引用 fill->SetOrientation(lightDir, upA, &rightA);
#else //BML+ / 老BML v0.3.2x：参数1/2 为指针 fill->SetOrientation(&lightDir, &upA, &rightA);
#endif
      fill->Active(TRUE);
      scene->AddObjectToScene(fill, FALSE);
      logger->Info("[VisualOpt] fill[%d] dir=(%.4f,%.4f,%.4f) color=%.4f",
        i, lightDir.x, lightDir.y, lightDir.z, fillStrength);
    }

    logger->Info("========== VisualOptimization lighting apply end ==========");
    m_bml->SendIngameMessage("Lighting Optimization Enabled");
  }

  // 清理补光 + 按名字恢复主光原始颜色（幂等；退出关卡/回菜单/关闭开关时调用）
  void cleanupLights() {
    CKContext* ctx = m_bml ? m_bml->GetCKContext() : NULL;
    if (!ctx) return;
    for (int i = 0; i < kFillCount; i++) {
      CKObject* old = ctx->GetObjectByName((char*)kFillNames[i]);
      if (old) ctx->DestroyObject(old);
    }
    if (main_color_saved && main_name[0]) {
      CKLight* main = (CKLight*)(ctx->GetObjectByName(main_name));
      if (main) {
        // 关键：只有主光颜色仍是我们设置的值时才恢复基准值（脚本设定的值）。
        // 若已被关卡脚本改过（如 12 关主光被脚本调暗），必须保持脚本当前值，
        // 否则会把暗主光恢复成亮色 -> 亮度翻倍。
        VxColor cur = main->GetColor();
        float d = fabsf(cur.r - main_original_color.r * kMainDim)
                + fabsf(cur.g - main_original_color.g * kMainDim)
                + fabsf(cur.b - main_original_color.b * kMainDim);
        if (d < 0.02f) {
          main->SetColor(main_original_color);
          if (GetLogger()) GetLogger()->Info("[VisualOpt] restored main \"%s\" to (%.2f,%.2f,%.2f)",
            main_name, main_original_color.r, main_original_color.g, main_original_color.b);
        } else {
          if (GetLogger()) GetLogger()->Info("[VisualOpt] main \"%s\" changed by level script (cur=(%.2f,%.2f,%.2f)), keep as-is",
            main_name, cur.r, cur.g, cur.b);
        }
      }
    }
    main_color_saved = false;
    last_main_intensity = 0.0f;
    if (GetLogger()) GetLogger()->Info("[VisualOpt] fill lights cleaned up");
  }

public:
  VisualOptimization(IBML* bml) : IMod(bml) {}

  virtual iCKSTRING GetID() override { return "VisualOptimization"; }
  virtual iCKSTRING GetVersion() override { return "1.0.0"; }
  virtual iCKSTRING GetName() override { return "VisualOptimization"; }
  virtual iCKSTRING GetAuthor() override { return "Entity_303-E3"; }
  virtual iCKSTRING GetDescription() override {
#ifdef CONFIG_MODE
    return "Merged from BallanceBug's ViewDistanceEditor (view distance). Two new features: preload all static meshes (no stutter) and backlight fill (brighten backlit surfaces). Configurable view distance, default 1200 (game default).";
#else
    return "Merged from BallanceBug's ViewDistanceEditor (view distance). Two new features: preload all static meshes (no stutter) and backlight fill (brighten backlit surfaces). View distance fixed at 1e9.";
#endif
  }
  DECLARE_BML_VERSION;

#ifdef CONFIG_MODE
  // 注册配置项（BML+ 配置界面显示为输入栏 / 开关）
  void OnLoad() override {
    GetConfig()->SetCategoryComment("Main", "Main settings.");
    char comment[96];

    prop_view_distance = GetConfig()->GetProperty("Main", "ViewDistance");
    prop_view_distance->SetDefaultFloat(DEFAULT_VIEW_DISTANCE);
    std::snprintf(comment, sizeof(comment), "View distance. Default: %.5g (game default)", (double)DEFAULT_VIEW_DISTANCE);
    prop_view_distance->SetComment(comment);

    prop_point_distance = GetConfig()->GetProperty("Main", "ExtraPointDistance");
    prop_point_distance->SetDefaultFloat(DEFAULT_POINT_DISTANCE);
    std::snprintf(comment, sizeof(comment), "Pickup/view distance of extra points. Default: %.5g", (double)DEFAULT_POINT_DISTANCE);
    prop_point_distance->SetComment(comment);

    prop_life_distance = GetConfig()->GetProperty("Main", "ExtraLifeDistance");
    prop_life_distance->SetDefaultFloat(DEFAULT_LIFE_DISTANCE);
    std::snprintf(comment, sizeof(comment), "Pickup/view distance of extra lives. Default: %.5g", (double)DEFAULT_LIFE_DISTANCE);
    prop_life_distance->SetComment(comment);

    prop_preload = GetConfig()->GetProperty("Main", "PreloadMeshes");
    prop_preload->SetDefaultBoolean(true);
    prop_preload->SetComment("Preload all static meshes on level start to avoid stutter.");

    prop_lighting = GetConfig()->GetProperty("Main", "LightingOptimization");
    prop_lighting->SetDefaultBoolean(true);
    prop_lighting->SetComment("Adds a weak backlight to surfaces facing away from the main light.");
  }
#endif

  // 确保配置已初始化。
  // 注意：游戏“启动后直接进关”（不经过主菜单）时 OnPostStartMenu 可能不触发，
  // 导致 init 保持 false，apply_all/OnProcess 全部被跳过（第一次进关无任何效果/消息）。
  // 因此 OnStartLevel 也会调用本函数兜底初始化。
  void ensureInit() {
    if (init) return;
#ifdef CONFIG_MODE
    if (prop_view_distance) {
      view_distance  = prop_view_distance->GetFloat();
      life_distance  = prop_life_distance->GetFloat();
      point_distance = prop_point_distance->GetFloat();
      preload_enabled  = prop_preload->GetBoolean();
      lighting_enabled = prop_lighting->GetBoolean();
    }
#endif
    init = true;
  }

  void OnPostStartMenu() override {
    cleanupLights();  // 每次回菜单都兜底清理补光
    ensureInit();
  }

  virtual void OnLoadObject(iCKSTRING filename, CKBOOL isMap, iCKSTRING masterName,
      CK_CLASSID filterClass, CKBOOL addtoscene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
      CKBOOL dynamic, XObjectArray* objArray, CKObject* masterObj) override {
    // 关卡/小节切换时会重新加载这些对象，此时需要重新应用距离设置
    if (_strcmpi(filename, "3D Entities\\PH\\P_Extra_Life.nmo") == 0) {
      apply_extra_life();
    }
    else if (_strcmpi(filename, "3D Entities\\PH\\P_Extra_Point.nmo") == 0) {
      apply_extra_point();
    }
  }

  void OnCamNavActive() override {
    apply_all();
  }

  void OnStartLevel() override {
    ensureInit();               // 启动直接进关时 OnPostStartMenu 可能未触发，这里兜底初始化
    notify = true;
    render_preloaded = false;   // 每关重新预渲染（切关后失效问题）
    lighting_applied = false;   // 每关重新应用光照
    lighting_frames  = 0;
    main_color_saved = false;   // 新关主光颜色重新读取
    last_main_intensity = 0.0f;
    cleanupLights();            // 清理残留补光
    apply_all();
  }

  void OnPostLoadLevel() override { apply_all(); }
  void OnPostResetLevel() override { apply_all(); }
  void OnPostNextLevel() override { apply_all(); }

  // 退出关卡：销毁补光，只让补光在关卡内生效
  void OnPreExitLevel() override { cleanupLights(); }

  // 兜底：逐帧检查视距；进关第1帧 preload；第3帧光照优化
  void OnProcess() override {
    if (!init || !m_bml->IsIngame()) return;
    auto cam = get_ingame_cam();
    if (cam && cam->GetBackPlane() != view_distance) {
      cam->SetBackPlane(view_distance); // 静默恢复，不刷屏
    }
    if (!render_preloaded && preload_enabled) {
      render_preloaded = true;
      prerender_meshes(m_bml->GetCKContext());
    }
    if (!lighting_applied && lighting_enabled) {
      if (++lighting_frames >= 3) {
        lighting_applied = true;
        applyLighting();
      }
    }
    // 自适应跟随：关卡脚本可能改变主光强度（如 12 关动态主光），补光强度同步更新
    if (lighting_applied && lighting_enabled) {
      CKContext* ctx = m_bml->GetCKContext();
      if (ctx) {
        CKLight* main = findMainLight(ctx);
        if (main) {
          const VxColor& mc = main->GetColor();
          float mi = (mc.r + mc.g + mc.b) / 3.0f * main->GetLightPower();
          if (mi <= 0.01f) mi = 1.0f;
          if (fabsf(mi - last_main_intensity) > 0.05f) {
            last_main_intensity = mi;
            // 脚本改动了主光（如 12 关进关后被脚本调暗）：把脚本当前值作为新基准，
            // 主光 = 脚本值 × kMainDim（整体倍率），补光同步跟随
            main_original_color = main->GetColor();
            main_color_saved = true;
            const char* mname2 = main->GetName();
            if (mname2) std::snprintf(main_name, sizeof(main_name), "%s", mname2); else main_name[0] = '\0';
            main->SetColor(VxColor(main_original_color.r * kMainDim,
                                    main_original_color.g * kMainDim,
                                    main_original_color.b * kMainDim));
            float ns = 2.0f * kBackAmbient * mi / (float)kFillCount;
            for (int i = 0; i < kFillCount; i++) {
              CKLight* fill = (CKLight*)(ctx->GetObjectByName((char*)kFillNames[i]));
              if (fill) fill->SetColor(VxColor(ns, ns, ns));
            }
            if (GetLogger()) GetLogger()->Info("[VisualOpt] main changed by script -> intensity %.3f, main=%.3fx%.3f, fill strength -> %.4f",
              mi, kMainDim, main_original_color.r, ns);
          }
        }
      }
    }
  }

  //老BML 0.3.43：IMod::OnPhysicalize/OnUnphysicalize 为 inline 空实现且 BML.dll 不导出，
  //MSVC 对 vtable 未生成 weak 符号导致链接失败，这里显式 override 空实现（BML+下不需要）
#ifndef USING_BML_PLUS
  void OnPhysicalize(CK3dEntity* target, CKBOOL fixed, float friction, float elasticity, float mass,
    CKSTRING collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter, float linearDamp,
    float rotDamp, CKSTRING collSurface, VxVector massCenter, int convexCnt, CKMesh** convexMesh,
    int ballCnt, VxVector* ballCenter, float* ballRadius, int concaveCnt, CKMesh** concaveMesh) override {}
  void OnUnphysicalize(CK3dEntity* target) override {}
#endif

#ifdef USING_BML_PLUS
  void OnPostCommandExecute(ICommand* command, const std::vector<std::string>& args) override {
    if (command && command->GetName() == "sector") {
      apply_all();
    }
  }
#endif

#ifdef CONFIG_MODE
  void OnModifyConfig(iCKSTRING category, iCKSTRING key, IProperty* prop) override {
    notify = true;
    view_distance  = prop_view_distance->GetFloat();
    life_distance  = prop_life_distance->GetFloat();
    point_distance = prop_point_distance->GetFloat();
    preload_enabled  = prop_preload->GetBoolean();
    lighting_enabled = prop_lighting->GetBoolean();
    // 光照优化开关实时生效：关闭立即移除补光；打开则下一帧重新应用
    if (!lighting_enabled) {
      cleanupLights();
      lighting_applied = false;
      m_bml->SendIngameMessage("Lighting Optimization Disabled");
    } else if (!lighting_applied) {
      lighting_applied = false; // 保持未应用，下一帧 OnProcess 应用
    }
    apply_all();
  }
#endif
};

IMod* BMLEntry(IBML* bml) {
  return new VisualOptimization(bml);
}

#ifdef USING_BML_PLUS
MOD_EXPORT void BMLExit(IMod* mod) {
  delete mod;
}
#endif
