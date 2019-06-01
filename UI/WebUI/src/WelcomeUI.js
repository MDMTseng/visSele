import React from "react";
import APP_ANALYSIS_MODE_rdx from "JSSRCROOT/AnalysisUI";

import  PageHeader  from 'antd/lib/page-header';
import  Typography  from 'antd/lib/typography';
const { Paragraph } = Typography;
// const { Header, Content, Footer, Sider } = Layout;

function WelcomeUI(){
    const content = (
        <div className="content">
            <Paragraph>
                HYVision 2019 is the most famous vision-check system in the world.
            </Paragraph>
            <Paragraph>
                <p>HYVision 2019 - v1 (http://hyv.idcircle.me)</p>
                <p>HYVision 2019 - v2 (http://hyv.idcircle.me)</p>
                <p>HYVision 2019 - v3 (http://hyv.idcircle.me)</p>
            </Paragraph>
            <p className="contentLink">
                <a>
                    <img
                        src="https://gw.alipayobjects.com/zos/rmsportal/MjEImQtenlyueSmVEfUD.svg"
                        alt="start"
                    /> Quick Start
                </a>
                <a>
                    <img src="https://gw.alipayobjects.com/zos/rmsportal/NbuDUAuBlIApFuDvWiND.svg" alt="info" />
                    Product Info
                </a>
                <a>
                    <img src="https://gw.alipayobjects.com/zos/rmsportal/ohOEPSYdDTNnyMbGuyLb.svg" alt="doc" />
                    Product Doc
                </a>
            </p>
        </div>
    );

    const extraContent = (
        <img
            src="https://gw.alipayobjects.com/mdn/mpaas_user/afts/img/A*KsfVQbuLRlYAAAAAAAAAAABjAQAAAQ/original"
            alt="content"
        />
    );
    const routes = [
        {
            path: 'index',
            breadcrumbName: 'HYV',
        },
        {
            path: 'first',
            breadcrumbName: 'HOME',
        },
        {
            path: 'second',
            breadcrumbName: 'Manual',
        },
    ];
    return (<PageHeader title="HYVision 2019" breadcrumb={{ routes }}>
        <div className="wrap">
            <div className="content">{content}</div>
            <div className="extraContent">{extraContent}</div>
        </div>
    </PageHeader>);
}
export default WelcomeUI;