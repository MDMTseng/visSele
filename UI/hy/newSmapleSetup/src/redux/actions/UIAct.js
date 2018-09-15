
export let UI_ACT_Type= {
    DROPDOWN_EXPEND: "MENU_EXPEND",
    BODY_PAGE_SWITCH: "BODY_PAGE_SWITCH",
    SPLASH_SWITCH:"SPLASH_SWITCH"
}


export let UI_BodyPage= {
    LOG: "LOG",
    RadarView: "RadarView",
    PokeSelectView: "PokeSelectView",
}

export function UIACT_SetMENU_EXPEND(ifExpand)
{
  return {
    type: UI_ACT_Type.DROPDOWN_EXPEND ,data:ifExpand
  }
}

export function UIACT_SwitchBodyPage(page)
{
  return {
    type: UI_ACT_Type.BODY_PAGE_SWITCH ,data:page
  }
}

export function UIACT_SPLASH_SWITCH(ifShow)
{
  return {
    type: UI_ACT_Type.SPLASH_SWITCH ,data:ifShow
  }
}
