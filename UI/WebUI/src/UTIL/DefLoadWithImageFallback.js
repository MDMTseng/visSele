const IMAGE_EXTENSIONS = [".png", ".jpg", ".jpeg", ".bmp"];

function hasImageExtension(path) {
  return /\.(png|jpe?g|bmp)$/i.test(path || "");
}

export function imgSrcCandidates(defModelPath) {
  if (hasImageExtension(defModelPath)) return [defModelPath];
  return [defModelPath, ...IMAGE_EXTENSIONS.map((ext) => defModelPath + ext)];
}

export function hasImagePacket(pkts) {
  return Array.isArray(pkts) && pkts.some((pkt) => pkt && pkt.type === "IM" && pkt.image);
}

export function loadDefWithImageFallback({
  defModelPath,
  defExtension,
  downSampLevel,
  send,
  timeoutMs = 5000,
}) {
  const candidates = imgSrcCandidates(defModelPath);
  let firstPkts = null;

  function tryAt(idx) {
    const imgsrc = candidates[idx];
    return new Promise((resolve, reject) => {
      let done = false;
      const timer = setTimeout(() => {
        if (done) return;
        done = true;
        reject("Timeout");
      }, timeoutMs);

      send(
        {
          deffile: defModelPath + "." + defExtension,
          imgsrc,
          down_samp_level: downSampLevel,
        },
        {
          resolve: (pkts, actionChannel) => {
            if (done) return;
            done = true;
            clearTimeout(timer);
            resolve({ pkts, actionChannel, imgsrc });
          },
          reject: (err) => {
            if (done) return;
            done = true;
            clearTimeout(timer);
            reject(err);
          },
        }
      );
    }).then(
      (result) => {
        if (firstPkts === null) firstPkts = result.pkts;
        if (hasImagePacket(result.pkts) || idx >= candidates.length - 1) return result;
        return tryAt(idx + 1).catch(() => result);
      },
      (err) => {
        if (idx >= candidates.length - 1) throw err;
        return tryAt(idx + 1);
      }
    );
  }

  return tryAt(0).then((result) => {
    if (!hasImagePacket(result.pkts) && firstPkts !== null) {
      return { ...result, pkts: firstPkts };
    }
    return result;
  });
}
