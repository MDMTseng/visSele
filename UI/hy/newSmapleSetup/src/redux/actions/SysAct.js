

export let SYS_ACT_Type= {
  SERVICE_STATUS_UPDATE: "SERVICE_STATUS_UPDATE",
  SERVICE_STATUS_RENEW: "SERVICE_STATUS_RENEW",
}




export function Act_StatusUpdate(notify)
{
  return {
    type: SYS_ACT_Type.SERVICE_STATUS_UPDATE,data:notify
  }
}

export function Act_StatusRenew(status)
{
  return {
    type: SYS_ACT_Type.SERVICE_STATUS,data:status
  }
}
