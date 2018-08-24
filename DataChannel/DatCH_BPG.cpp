
#include "DatCH_BPG.hpp"

DatCH_BPG1_0::DatCH_BPG1_0(ws_conn_data *conn): DatCH_BPG()
{
  version="1.0.0.alpha";
  RESET(conn);
}

void DatCH_BPG1_0::RESET(ws_conn_data *conn)
{
  state = BPG_END;
  this->conn = conn;
}

DatCH_BPG1_0::~DatCH_BPG1_0()
{

}


DatCH_Data DatCH_BPG1_0::SendHR()
{

}
