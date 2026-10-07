import crypto from 'node:crypto';
import fs from 'node:fs/promises';
import https from 'node:https';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const htmlDir = path.join(__dirname, 'html');

const mimeTypes = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.webmanifest': 'application/manifest+json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png'
};

// Auto-load .env files from tools/.env or root .env
async function loadEnvFiles() {
  const envCandidates = [
    path.join(__dirname, '.env'),
    path.join(__dirname, '..', '.env')
  ];

  for (const envPath of envCandidates) {
    try {
      if (typeof process.loadEnvFile === 'function') {
        try { process.loadEnvFile(envPath); } catch {}
      } else {
        const content = await fs.readFile(envPath, 'utf8');
        for (const line of content.split(/\r?\n/)) {
          const trimmed = line.trim();
          if (!trimmed || trimmed.startsWith('#')) continue;
          const eqIdx = trimmed.indexOf('=');
          if (eqIdx !== -1) {
            const key = trimmed.slice(0, eqIdx).trim();
            let val = trimmed.slice(eqIdx + 1).trim();
            if ((val.startsWith('"') && val.endsWith('"')) || (val.startsWith("'") && val.endsWith("'"))) {
              val = val.slice(1, -1);
            }
            if (!process.env[key]) {
              process.env[key] = val;
            }
          }
        }
      }
    } catch {}
  }
}

function getEnvOrArg(envVar, flagName, defaultValue = '') {
  const argIndex = process.argv.indexOf(flagName);
  if (argIndex !== -1 && process.argv[argIndex + 1]) {
    return process.argv[argIndex + 1];
  }
  return process.env[envVar] || defaultValue;
}

function getConfig() {
  const accessKeyId = getEnvOrArg('AWS_ACCESS_KEY_ID', '--access-key-id');
  const secretAccessKey = getEnvOrArg('AWS_SECRET_ACCESS_KEY', '--secret-access-key');
  return {
    bucket: getEnvOrArg('S3_BUCKET', '--bucket'),
    region: getEnvOrArg('AWS_REGION', '--region', 'us-east-1'),
    accessKeyId,
    secretAccessKey,
    apiAccessKeyId: getEnvOrArg('LIGHTSAIL_API_KEY_ID', '--api-key-id', accessKeyId),
    apiSecretAccessKey: getEnvOrArg('LIGHTSAIL_API_SECRET_KEY', '--api-secret-key', secretAccessKey),
    distributionId: getEnvOrArg('CLOUDFRONT_DISTRIBUTION_ID', '--distribution-id'),
    lightsailDistribution: getEnvOrArg('LIGHTSAIL_DISTRIBUTION_NAME', '--lightsail-distro'),
    endpoint: getEnvOrArg('S3_ENDPOINT', '--endpoint'),
    makePublic: process.argv.includes('--public') || process.env.S3_PUBLIC === 'true'
  };
}

function hmac(key, data) {
  return crypto.createHmac('sha256', key).update(data).digest();
}

function hash(data) {
  return crypto.createHash('sha256').update(data).digest('hex');
}

function getSigningKey(secretKey, date, region = 'us-east-1', service = 's3') {
  const kDate = hmac(`AWS4${secretKey}`, date);
  const kRegion = hmac(kDate, region);
  const kService = hmac(kRegion, service);
  return hmac(kService, 'aws4_request');
}

async function invalidateCloudFrontCache({ distributionId, accessKeyId, secretAccessKey }) {
  const host = 'cloudfront.amazonaws.com';
  const pathUrl = `/2020-05-31/distribution/${distributionId}/invalidation`;

  const callerRef = String(Date.now());
  const payloadXml = `<InvalidationBatch xmlns="http://cloudfront.amazonaws.com/doc/2020-05-31/"><Paths><Quantity>1</Quantity><Items><Path>/*</Path></Items></Paths><CallerReference>${callerRef}</CallerReference></InvalidationBatch>`;

  const now = new Date();
  const datetime = now.toISOString().replace(/[:-]|\.\d{3}/g, '');
  const date = datetime.slice(0, 8);

  const payloadHash = hash(payloadXml);
  const contentType = 'application/xml';

  const canonicalHeaders =
    `content-type:${contentType}\n` +
    `host:${host}\n` +
    `x-amz-content-sha256:${payloadHash}\n` +
    `x-amz-date:${datetime}\n`;

  const signedHeaders = 'content-type;host;x-amz-content-sha256;x-amz-date';

  const canonicalRequest = [
    'POST',
    pathUrl,
    '',
    canonicalHeaders,
    signedHeaders,
    payloadHash
  ].join('\n');

  const credentialScope = `${date}/us-east-1/cloudfront/aws4_request`;
  const stringToSign = [
    'AWS4-HMAC-SHA256',
    datetime,
    credentialScope,
    hash(canonicalRequest)
  ].join('\n');

  const signingKey = getSigningKey(secretAccessKey, date, 'us-east-1', 'cloudfront');
  const signature = crypto.createHmac('sha256', signingKey).update(stringToSign).digest('hex');

  const authorization = `AWS4-HMAC-SHA256 Credential=${accessKeyId}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;

  return new Promise((resolve, reject) => {
    const req = https.request({
      hostname: host,
      path: pathUrl,
      method: 'POST',
      headers: {
        'Content-Type': contentType,
        'Content-Length': Buffer.byteLength(payloadXml),
        'Host': host,
        'x-amz-date': datetime,
        'x-amz-content-sha256': payloadHash,
        'Authorization': authorization
      }
    }, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          const invIdMatch = body.match(/<Id>([^<]+)<\/Id>/);
          resolve(invIdMatch ? invIdMatch[1] : 'Success');
        } else {
          reject(new Error(`HTTP ${res.statusCode}: ${body}`));
        }
      });
    });

    req.on('error', reject);
    req.write(payloadXml);
    req.end();
  });
}

async function resetLightsailCache({ lightsailDistribution, region, apiAccessKeyId, apiSecretAccessKey }) {
  const host = `lightsail.${region || 'us-east-1'}.amazonaws.com`;
  const bodyJson = JSON.stringify({ distributionName: lightsailDistribution });

  const now = new Date();
  const datetime = now.toISOString().replace(/[:-]|\.\d{3}/g, '');
  const date = datetime.slice(0, 8);
  const payloadHash = hash(bodyJson);

  const contentType = 'application/x-amz-json-1.1';
  const target = 'Lightsail_20161128.ResetDistributionCache';

  const canonicalHeaders =
    `content-type:${contentType}\n` +
    `host:${host}\n` +
    `x-amz-content-sha256:${payloadHash}\n` +
    `x-amz-date:${datetime}\n` +
    `x-amz-target:${target}\n`;

  const signedHeaders = 'content-type;host;x-amz-content-sha256;x-amz-date;x-amz-target';

  const canonicalRequest = [
    'POST',
    '/',
    '',
    canonicalHeaders,
    signedHeaders,
    payloadHash
  ].join('\n');

  const credentialScope = `${date}/${region || 'us-east-1'}/lightsail/aws4_request`;
  const stringToSign = [
    'AWS4-HMAC-SHA256',
    datetime,
    credentialScope,
    hash(canonicalRequest)
  ].join('\n');

  const signingKey = getSigningKey(apiSecretAccessKey, date, region || 'us-east-1', 'lightsail');
  const signature = crypto.createHmac('sha256', signingKey).update(stringToSign).digest('hex');
  const authorization = `AWS4-HMAC-SHA256 Credential=${apiAccessKeyId}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;

  return new Promise((resolve, reject) => {
    const req = https.request({
      hostname: host,
      path: '/',
      method: 'POST',
      headers: {
        'Content-Type': contentType,
        'Content-Length': Buffer.byteLength(bodyJson),
        'Host': host,
        'x-amz-date': datetime,
        'x-amz-target': target,
        'x-amz-content-sha256': payloadHash,
        'Authorization': authorization
      }
    }, (res) => {
      let resBody = '';
      res.on('data', chunk => resBody += chunk);
      res.on('end', () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve({ statusCode: res.statusCode });
        } else {
          reject(new Error(`HTTP ${res.statusCode}: ${resBody}`));
        }
      });
    });

    req.on('error', reject);
    req.write(bodyJson);
    req.end();
  });
}

async function uploadFile({ bucket, region, accessKeyId, secretAccessKey, endpoint, makePublic, objectKey, content, contentType, cacheControl }) {
  let host = endpoint ? endpoint.replace(/^https?:\/\//, '') : `${bucket}.s3.${region}.amazonaws.com`;
  const protocol = endpoint && endpoint.startsWith('http://') ? http : https;

  const urlPath = `/${objectKey}`;

  const now = new Date();
  const datetime = now.toISOString().replace(/[:-]|\.\d{3}/g, '');
  const date = datetime.slice(0, 8);

  const payloadHash = hash(content);

  const headersToSign = {
    'cache-control': cacheControl,
    'content-type': contentType,
    'host': host,
    'x-amz-content-sha256': payloadHash,
    'x-amz-date': datetime
  };

  if (makePublic) {
    headersToSign['x-amz-acl'] = 'public-read';
  }

  const sortedHeaderKeys = Object.keys(headersToSign).sort();
  const canonicalHeaders = sortedHeaderKeys.map(k => `${k}:${headersToSign[k]}\n`).join('');
  const signedHeaders = sortedHeaderKeys.join(';');

  const canonicalRequest = [
    'PUT',
    urlPath,
    '',
    canonicalHeaders,
    signedHeaders,
    payloadHash
  ].join('\n');

  const credentialScope = `${date}/${region}/s3/aws4_request`;
  const stringToSign = [
    'AWS4-HMAC-SHA256',
    datetime,
    credentialScope,
    hash(canonicalRequest)
  ].join('\n');

  const signingKey = getSigningKey(secretAccessKey, date, region, 's3');
  const signature = crypto.createHmac('sha256', signingKey).update(stringToSign).digest('hex');

  const authorization = `AWS4-HMAC-SHA256 Credential=${accessKeyId}/${credentialScope}, SignedHeaders=${signedHeaders}, Signature=${signature}`;

  const reqHeaders = {
    'Content-Type': contentType,
    'Content-Length': content.length,
    'Cache-Control': cacheControl,
    'Host': host,
    'x-amz-date': datetime,
    'x-amz-content-sha256': payloadHash,
    'Authorization': authorization
  };

  if (makePublic) {
    reqHeaders['x-amz-acl'] = 'public-read';
  }

  return new Promise((resolve, reject) => {
    const req = protocol.request({
      hostname: host.split(':')[0],
      port: host.includes(':') ? Number(host.split(':')[1]) : undefined,
      path: urlPath,
      method: 'PUT',
      headers: reqHeaders
    }, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        if (res.statusCode >= 200 && res.statusCode < 300) {
          resolve({ statusCode: res.statusCode });
        } else {
          reject(new Error(`HTTP ${res.statusCode}: ${body || res.statusMessage}`));
        }
      });
    });

    req.on('error', reject);
    req.write(content);
    req.end();
  });
}

async function main() {
  await loadEnvFiles();
  const config = getConfig();

  console.log('=== Lighthouse PWA S3 Deployer ===\n');

  if (!config.bucket || !config.accessKeyId || !config.secretAccessKey || config.bucket === 'your-lighthouse-pwa-bucket' || config.accessKeyId === 'your-access-key-id') {
    console.error('Error: Missing or default credentials in tools/.env');
    console.error('\nPlease edit tools/.env with your S3 bucket name and credentials:');
    console.error('  S3_BUCKET=my-bucket-name');
    console.error('  AWS_ACCESS_KEY_ID=AKIA...');
    console.error('  AWS_SECRET_ACCESS_KEY=...\n');
    console.error('Or pass them via CLI options:');
    console.error('  node tools/deploy-s3.mjs --bucket <bucket-name> --access-key-id <key> --secret-access-key <secret> [--public]\n');
    process.exit(1);
  }

  const files = await fs.readdir(htmlDir);
  const deployableFiles = files.filter(f => !f.endsWith('.test.mjs') && !f.endsWith('.md'));

  console.log(`Target Bucket : ${config.bucket}`);
  console.log(`Target Region : ${config.region}`);
  console.log(`Public ACL    : ${config.makePublic ? 'Enabled (public-read)' : 'Disabled'}`);
  if (config.lightsailDistribution) {
    console.log(`Lightsail CDN : ${config.lightsailDistribution}`);
  } else if (config.distributionId) {
    console.log(`CloudFront ID : ${config.distributionId}`);
  }
  console.log(`Uploading ${deployableFiles.length} assets from tools/html/...\n`);

  for (const filename of deployableFiles) {
    const filePath = path.join(htmlDir, filename);
    const content = await fs.readFile(filePath);
    const ext = path.extname(filename).toLowerCase();
    const contentType = mimeTypes[ext] || 'application/octet-stream';
    
    // Cache control: no-cache for HTML and Service Worker so updates apply instantly
    const cacheControl = (filename === 'index.html' || filename === 'sw.js')
      ? 'no-cache, no-store, must-revalidate'
      : 'public, max-age=31536000, immutable';

    try {
      process.stdout.write(`Uploading ${filename.padEnd(24)} (${(content.length / 1024).toFixed(1)} KB)... `);
      await uploadFile({
        ...config,
        objectKey: filename,
        content,
        contentType,
        cacheControl
      });
      console.log('✔ Done');
    } catch (err) {
      console.log('✖ Failed');
      console.error(`  Error uploading ${filename}: ${err.message}`);
      process.exitCode = 1;
    }
  }

  if (process.exitCode !== 1) {
    if (config.lightsailDistribution) {
      process.stdout.write('\nClearing Lightsail CDN cache... ');
      try {
        await resetLightsailCache(config);
        console.log('✔ Cache reset successfully');
      } catch (err) {
        console.log(`✖ Lightsail cache reset failed: ${err.message}`);
      }
    } else if (config.distributionId) {
      process.stdout.write('\nClearing CloudFront CDN cache (invalidation /*)... ');
      try {
        const invId = await invalidateCloudFrontCache(config);
        console.log(`✔ Invalidation created (ID: ${invId})`);
      } catch (err) {
        console.log(`✖ Cache invalidation failed: ${err.message}`);
      }
    }

    const s3Url = config.endpoint
      ? `${config.endpoint}/${config.bucket}/index.html`
      : `https://${config.bucket}.s3.${config.region}.amazonaws.com/index.html`;
    console.log(`\n🎉 Deployment complete! Access your PWA at:\n   ${s3Url}\n`);
  }
}

main().catch(err => {
  console.error('Fatal deployment error:', err);
  process.exit(1);
});
