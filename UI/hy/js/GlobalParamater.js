const RxMsg = {
    newnew: function () {
        var VARs = {};
        VARs.HR = null;
        VARs.SS = null;
        VARs.IM = null;
        VARs.IR = null;
        VARs.RAWIMAGE = null;
        VARs.after_IR = function () {
            initDataTables();
            initJsonPainter();
        };
        VARs.listAll = function () {
            console.log("[RxMsg][HR]");
            console.log(VARs.HR);
            console.log("[RxMsg][SS]");
            console.log(VARs.SS);
            console.log("[RxMsg][IM]");
            console.log(VARs.IM);
            console.log("[RxMsg][IR]");
            console.log(VARs.IR);
        };

        return VARs;
    }
};
const RXMSG = RxMsg.newnew();

const FilterVars = {
    newnew: function () {
        var VARs = {};
        VARs.val = 0.9;
        VARs.val2 = 0.9;
        VARs.updateGrayLevelVal = function (v) {
            if (v > 0 && v < 1)
                VARs.val = v;
        };
        VARs.getGrayLevelVal = function () {
            return VARs.val;
        };
        return VARs;
    }
};
const FV = FilterVars.newnew();

const Mouse_Vars = {
    newnew: function () {
        var VARs = {};
        var evt1={offsetX:0,offsetY:0};
        var evt2={offsetX:0,offsetY:0};
        var evt3={offsetX:0,offsetY:0};
        VARs.invX=0;
        VARs.invY=0;
        VARs.mouseMove_evts=[evt1,evt2,evt3];
        VARs.updateMouseEvt = function (evt,which) {
            if(which>0&&which<3)
            VARs.mouseMove_evts[which]=evt;
        };
        VARs.getMouseEvt = function (which) {
            return VARs.mouseMove_evts[which];
        };
        VARs.getMouseXY = function (which) {
            if(VARs.mouseMove_evts[which]===null)return;
            return [VARs.mouseMove_evts[which].offsetX,VARs.mouseMove_evts[which].offsetY];
        };
        return VARs;
    }
};
const MOUSE_VARs = Mouse_Vars.newnew();

const GlobalDrawArgs = {
    newnew: function () {
        let dx500ms = Math.PI*2 / 500;
        let millisX = 0;
        let VARs = {};
        VARs.FPS=0;
        VARs.fadingWB_1000ms = lerpColor('#ff0000', '#ffffff', 0.5);
        VARs.fadingDarkRed_1000ms = lerpColor('#550000', '#000000', 0.5);
        VARs.sin_0_1_1000ms=0;
        VARs.updateAllArgs = function (which) {
            millisX = performance.now();
            VARs.sin_0_1_1000ms=(1 + Math.sin(dx500ms * millisX))/2;
            VARs.fadingWB_1000ms = lerpColor('#000000', '#ffffff', VARs.sin_0_1_1000ms*0.1);
            VARs.fadingDarkRed_1000ms = lerpColor('#550000', '#000000', VARs.sin_0_1_1000ms);

        };
        return VARs;
    }
};
const GDA = GlobalDrawArgs.newnew();


