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
