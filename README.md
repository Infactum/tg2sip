![Build status](https://github.com/Infactum/tg2sip/workflows/CI/badge.svg)

# TG2SIP

TG2SIP is a Telegram<->SIP voice gateway. It can be used to forward incoming telegram calls to your SIP PBX or make SIP->Telegram calls.

## Requirements

Your SIP PBX should be comaptible with `L16@48000` or `OPUS@48000` voice codec.

## Usage

1. Obtain binaries in one of convenient ways for you.
   *  Build them from source.  
      Requires C++17 supported comiler, which may be a trouble for old linux distros.
   *  Download prebuild native binaries for one of supported distros.  
      Prebuild binaries requires OPUS libraries (`libopus0` for Ubuntu, `opus` for CentOS, etc).
   *  Download universal AppImage package.  
      More information of what is AppImage can be found here https://appimage.org/
      
2. Obtain `api_id` and `api_hash` tokens from [this](https://my.telegram.org) page and put them in `settings.ini` file.
3. Login into telegram with `gen_db` app
4. Set SIP server settings in `settings.ini`
5. Run `tg2sip`

SIP->Telegram calls can be done using 3 extension types:

1. `tg#[\s\d]+` for calls by username
2. `\+[\d]+` for calls by phone number
3. `[\d]+` for calls by telegram ID. Only known IDs allowed by telegram API.

All Telegram->SIP calls will be redirected to `callback_uri` SIP-URI that can be set in from `settings.ini` file.  
Extra information about caller Telegram account will be added into `X-TG-*` SIP tags.

## Donate

[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=755FZWPRC9YGL&lc=US&item_name=TG2SIP&currency_code=USD&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted)

[Yandex.Money](https://yasobe.ru/na/tg2sip)
