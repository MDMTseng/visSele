
export default {

    defConf:{
      calc_add_measure:"增加量測變數",
      
      exit_warning_change_is_made:"設定已更動 確定要離開嗎？",
      do_you_want_to_reset_def:"確定要重新設定嗎？",
      lock_level:"鎖等級",
      setup:"設定",
    },

    _:{
        // Primitive-editing vocabulary, shared by every property sheet.
        //
        // These fields were on screen in English -- locating, min_strength,
        // include_range, manual_offset -- on a machine operated in Chinese. A
        // setting nobody can read is a setting nobody adjusts, which is how
        // min_strength ended up at whatever the default was on every def in the
        // field.
        //
        // Here rather than under line/arc/search_point: they mean the same thing
        // in all three, and three copies is three chances to drift.
        locating:"定位方式",
        caliper:"卡尺",
        edge:"邊緣",
        count:"卡尺數",
        min_inliers:"最少有效點",
        max_error:"容許殘差",
        method:"選邊規則",
        polarity:"邊緣明暗",
        nth:"第幾個",
        min_strength:"邊緣強度門檻",
        include_range:"納入範圍",
        manual_offset:"人工偏移",
        rel_strength:"相對強度門檻",
        // Dropdown VALUES. Prefixed because the stored value stays English and
        // some of them collide with a field name ("nth" is both a rule and a
        // field).
        opt_contour:"輪廓",
        opt_caliper:"卡尺",
        opt_strongest:"最強",
        opt_first:"最近",
        opt_last:"最遠",
        opt_middle:"中間",
        opt_nth:"第 n 個",
        // WHAT THE SCAN CROSSES, not a word for a sign.
        //
        // "rising"/"falling" name the sign of the gradient, which is a fact
        // about the arithmetic. What the operator can check against the picture
        // is the brightness change along the search: on this backlit station the
        // part is dark on a bright field, so the silhouette's outer edge is the
        // white-to-black one. Deliberately no 外緣/內緣 note -- the polarity is
        // relative to the SEARCH DIRECTION, so which side is "outer" depends on
        // which way the arrow points, and a hint that is right half the time is
        // worse than the transition stated plainly.
        opt_any:"不限",
        opt_rising:"黑→白",
        opt_falling:"白→黑",
        pick_ref:"（點選）",
        // Envelope fit: the centre is least-squares either way, the RADIUS is
        // the mean, the largest, or the smallest |centre-hit|. "平均/外包/內包"
        // is what the shop calls them.
        fit_mode:"擬合方式",
        opt_ls:"平均",
        opt_outer:"外包",
        opt_inner:"內包",
        // The line's two envelope sides. Which one is "front" is fixed by the
        // direction the def drew the line (p0->p1), so it is a thing to try and
        // see rather than a thing to reason about from the words.
        opt_front:"前凸點",
        opt_back:"後凸點",
        ERROR:"錯誤",
        ERROR_INFO:"系統訊息",
        WARNING:"警告",
        line:"線段/Line",
        arc:"弧/Arc",
        spoint:"搜尋點/SPOINT",
        search_point:"搜尋點",
        save_calibration:"儲存校正",
        
        apoint:"交點/APOINT",
        aux_point:"交點",
        '<':"返回",
        measure:"測量/MEASURE",
        edit:"編輯測量/Edit",
        save:"儲存/SAVE",
        load:"讀取/LOAD",
        take:"重新設定/TAKE",
        type:"類型",
        subtype:"次類型",
        value:"出貨目標",
        margin:"範圍",
        line_thickness_value:"線寬數值",
        submargin1:"生產目標",
        margin2:"次範圍",
        direction:"方向",
        angleDeg:"角度°",
        width:"寬",
        ref:"參考物件",
        ref_baseLine:"參考線",
        id:"編號",
        sigma:"標準差",
        distance:"距離/D",
        angle:"角度/A",
        radius:"半徑/R",
        calc:"計算/C",
        name:"名稱",
        importance:"重要等級",
        DefFileName:"設定檔名",
        DefFileTag:"標籤",
        "Info Graphs":"資料圖表",
        matchingAngleLimit180:"角度限制180",
        matchingAngleLimit10:"角度限制10",
        matchingFaceFrontOnly:"僅正面",
        setting:"設定",


        system_status:"系統狀態",
        connection_status:"連線狀態",
        core:"運算核心",
        camera:"檢測相機",
        boot_daemon:"啟動總管",
        upload_database:"資料庫",

        vertex_touch_searching:"凸點連線",
        update_status:"更新狀態",

        force_update:"強制更新",
        normal_update:"更新",


        save_machine_setting:"儲存機器設定",
        disconnected:"已斷線",
        connected:"已連線",
        camera_reconnection_caption:"相機重連中...",
        uInsp_reconnection_caption:"全檢設備重連中...",
        manual_ROI_setup:"手動視野範圍(ROI)選擇",
        uInsp_ctrl:"全檢機控制",
        ERROR_CODES:"系統碼",
        ERROR_CLEAR:"清除系統碼",
        SPEED_SET:"速度設定",
        SET_DEFAULT_RPM:"設定預設RPM",        
        RESET_INSPECTION_COUNTER:"重設檢測計數",
        uInsp_ACTION_TRIGGER_TIMING:"檢測觸發時間",

        TEST_MODE:"測試模式",
        TEST_MODE_NORMAL:"回復正常",
        TEST_MODE_INC:"逐項測試",
        TEST_MODE_NO_BLOW:"無噴氣",
        TEST_MODE_ALTER_BLOW:"交錯噴氣",
        TEST_MODE_DISCONNECT:"關閉連線"
    },
    connection:{
      server_connected:"已連結",
      server_disconnected:"斷線!! 數據不會上傳",
      server_disconnecting:"嘗試連線中",
      connect:"連線"
    },
    measure:{
      quality_essential:"品質必需",
      NGasNA:"NG→NA",
      NAasNG:"NA→NG",
      orientation_essential:"朝向必需",

      value_adjust:"數值加",
      value_mult:"數值乘",
      value_adjust_b:"數值加(背)",
      value_mult_b:"數值乘(背)",
      back_value_setup:"背面目標設定",
      value_b:"目標(背)",
      USL_b:"規格上限(背)",
      LSL_b:"規格下限(背)",
      UCL_b:"管制上限(背)",
      LCL_b:"管制下限(背)",
      value:"目標",
      USL:"規格上限",
      LSL:"規格下限",
      UCL:"管制上限",
      LCL:"管制下限",

      
      UST:"上規格公差",
      LST:"下規格公差",
      UCT:"上管制公差",
      LCT:"下管制公差",

      importance:"重要等級",
      quadrant:"量測象限",

      // The rest of the measure sheet. The limits already had words here and
      // the sheet was printing the keys anyway -- USL/LSL/UCL/LCL were on
      // screen in English next to a dictionary that has said 規格上限 all along.
      target:"目標與規格",
      back:"背面",
      behavior:"判定行為",
      value_mapping:"數值換算",
      value_A:"換算 A",
      value_B:"換算 B",
      value_X:"換算 X",
      value_Y:"換算 Y",
      baseLine:"基準線",
      calc_f:"運算式",
      // Subtypes, shown translated and stored as they are.
      opt_distance:"距離",
      opt_angle:"角度",
      opt_radius:"半徑",
      opt_calc:"運算",
      opt_circle_info:"圓資訊",
      opt_max_diameter:"最大直徑",
      opt_min_diameter:"最小直徑",
      opt_roughness_max:"最大粗糙度",
      opt_roughness_min:"最小粗糙度",
      opt_roughness_rmse:"粗糙度 RMSE",
      opt_NA:"未設定"
    },
    search_point:{
      search_far:"近/遠",
      locating_anchor:"定位錨點",
      anchor_corner:"角點(2D)",
    },
    mainui:{
      select_deffile:"選擇定義檔",
      select_deffile_detail:"",
      set_insp_tags:"設定檢測標籤",
      set_insp_tags_detail:"",
      GOGOGO:"執行檢測",
      GOGOGO_detail:"",

      MODE_SELECT_MAIN_MENU:"主選單",
      MODE_SELECT_DEFCONF:"量測設定",
      MODE_SELECT_INSP_PREP:"檢驗準備",
      MODE_SELECT_REP_DISPLAY:"檢驗回視",
      MODE_SELECT_PRECISION_VALIDATION:"精度校正",
      MODE_SELECT_INST_INSP:"簡驗",
      MODE_SELECT_BACKLIGHT_CALIB:"背光校正",
      MODE_SELECT_CALIBRATION:"相機校正",
      MODE_SELECT_SETTING:"設定",
      FILE_NOT_FOUNT:"找不到檔案",

      FUNC_auto_recognition:"自動比對",
      FUNC_auto_recognition_running:"自動比對中",
    }

};